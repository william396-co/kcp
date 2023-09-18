#pragma once

#include <cstdint>
#include <list>
#include <functional>

//---------------------------------------------------------------------
// BYTE ORDER & ALIGNMENT
//---------------------------------------------------------------------
#ifndef IWORDS_BIG_ENDIAN
#ifdef _BIG_ENDIAN_
#if _BIG_ENDIAN_
#define IWORDS_BIG_ENDIAN 1
#endif
#endif
#ifndef IWORDS_BIG_ENDIAN
#if defined(__hppa__) || \
            defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
            (defined(__MIPS__) && defined(__MIPSEB__)) || \
            defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
            defined(__sparc__) || defined(__powerpc__) || \
            defined(__mc68000__) || defined(__s390x__) || defined(__s390__)
#define IWORDS_BIG_ENDIAN 1
#endif
#endif
#ifndef IWORDS_BIG_ENDIAN
#define IWORDS_BIG_ENDIAN  0
#endif
#endif

#ifndef IWORDS_MUST_ALIGN
#if defined(__i386__) || defined(__i386) || defined(_i386_)
#define IWORDS_MUST_ALIGN 0
#elif defined(_M_IX86) || defined(_X86_) || defined(__x86_64__)
#define IWORDS_MUST_ALIGN 0
#elif defined(__amd64) || defined(__amd64__)
#define IWORDS_MUST_ALIGN 0
#else
#define IWORDS_MUST_ALIGN 1
#endif
#endif

struct kcpSeg
{
	uint32_t conv;
	uint32_t cmd;
	uint32_t frg;
	uint32_t wnd;
	uint32_t ts;// timestamp
	uint32_t sn;
	uint32_t una;
	uint32_t len;
	uint32_t resendts; //Resend timestamp
	uint32_t rto;
	uint32_t fastack;// Fast Ack
	uint32_t xmit;
	char* data;

	explicit kcpSeg(int size) :
		kcpSeg(size, 0, 0, 0, 0, 0, 0) {}

	kcpSeg(int size, uint32_t conv_, uint32_t cmd_, uint32_t frg_, uint32_t wnd_, uint32_t ts_, uint32_t una_) :
		conv{ conv_ }, cmd{ cmd_ }, frg{ frg_ },
		wnd{ wnd_ }, ts{ ts_ }, sn{ 0 }, una{ una_ },
		len{ (uint32_t)size }, resendts{ 0 }, rto{ 0 },
		fastack{ 0 }, xmit{ 0 },data{nullptr}
	{
		if (len > 0) {
			data = new char[size];
		}
	}
	~kcpSeg() {
		delete[] data;
	}

};

using SeqQueue = std::list<kcpSeg*>;
class Kcp;
using OutputFn = std::function<int(const char* buf, int len, Kcp* kcp, void* user)>;
using WriteLogFn = std::function<void(const char* log, Kcp* kcp, void* user)>;


enum Log_Mask
{
	log_Output = 0x1 << 0,
	log_Input = 0x1 << 1,
	log_Send = 0x1 << 2,
	log_Recv = 0x1 << 3,
	log_In_Data = 0x1 << 4,
	log_In_Ack = 0x1 << 5,
	log_In_Probe = 0x1 << 6,
	log_In_Wins = 0x1 << 7,
	log_Out_Data = 0x1 << 8,
	log_Out_Ack = 0x1 << 9,
	log_Out_Probe = 0x1 << 10,
	log_Out_Wins = 0x1 << 11
};

class Kcp
{
public:
	explicit Kcp(uint32_t conv_, void* user_);
	~Kcp();

	// Disallow copy and move
	Kcp(Kcp const&) = delete;
	Kcp& operator=(Kcp const&) = delete;
	Kcp(Kcp&&) = delete;
	Kcp& operator=(Kcp&&) = delete;

	// set output callback, which will be invoked by kcp
	void set_output(OutputFn const& fn) {
		on_output_ = fn;
	}

	// user/upper level recv: returns size, returns below zero for EAGAIN
	int recv(char* buffer_, int len_);
	// user/upper level send, returns below zero for error
	int send(const char* data_, int len_);

	// update state (call it repeatedly, every 10ms-100ms), or you can ask 
	// ikcp_check when to call it again (without ikcp_input/_send calling).
	// 'current' - current timestamp in millisec. 
	void update(uint32_t current_);

	// Determine when should you invoke ikcp_update:
	// returns when you should invoke ikcp_update in millisec, if there 
	// is no ikcp_input/_send calling. you can call ikcp_update in that
	// time, instead of call update repeatly.
	// Important to reduce unnacessary ikcp_update invoking. use it to 
	// schedule ikcp_update (eg. implementing an epoll-like mechanism, 
	// or optimize ikcp_update when handling massive kcp connections)
	uint32_t check(uint32_t current_);

	// when you received a low level packet (eg. UDP packet), call it
	int input(const char* data, long size);

	// flush pending data
	void flush();
	
	// check the size of next message in the recv queue
	int peeksize();

	// change MTU size, default is 1400
	int setmtu(int mtu_);

	// set maximum window size: sndwnd=32, rcvwnd=32 by default
	int set_wndsize(int sndwnd_, int rcvwnd_);

	// get how many packet is waiting to be sent
	int waitsnd();

	// fastest: ikcp_nodelay(kcp, 1, 20, 2, 1)
	// nodelay: 0:disable(default), 1:enable
	// interval: internal update timer interval in millisec, default is 100ms 
	// resend: 0:disable fast resend(default), 1:enable fast resend
	// nc: 0:normal congestion control(default), 1:disable congestion control
	int set_nodelay(int nodelay_, int interval_, int resend_, int nc_);
public:
	void set_rx_minrto(int rx_minrto_) { rx_minrto = rx_minrto_; }
	void set_fastresend(int fastresend_) { fastresend = fastresend_; }
private:	
	void write_log(int mask, const char* fmt, ...);

	int setinterval(int interval_);

	int output(const char* data_, int size_);

	// check log mask
	bool canlog(int mask_);
	//---------------------------------------------------------------------
	// ikcp_encode_seg
	//---------------------------------------------------------------------
	char* encode_seg(char* ptr_, const kcpSeg* seg_);

	int wnd_unused();

	//---------------------------------------------------------------------
	// ack append
	//---------------------------------------------------------------------
	void ack_push(uint32_t sn_, uint32_t ts_);

	void ack_get(int p, uint32_t* sn_, uint32_t* ts_);

	//---------------------------------------------------------------------
	// parse data
	//---------------------------------------------------------------------
	void parse_data(kcpSeg* newseg_);
 
	//---------------------------------------------------------------------
	// parse ack
	//---------------------------------------------------------------------
	void update_ack(int32_t rtt_);

	void shrink_buf();

	void parse_ack(uint32_t sn_);

	void parse_una(uint32_t una_);

	void parse_fastack(uint32_t sn_, uint32_t ts_);

	bool valide_cmd(uint8_t cmd_);
private:
	uint32_t conv, mtu, mss, state;
	uint32_t snd_una, snd_nxt, rcv_nxt;
	uint32_t ts_recent, ts_lastack, ssthresh;
	int32_t rx_rttval, rx_srtt, rx_rto, rx_minrto;
	uint32_t snd_wnd, rcv_wnd, rmt_wnd, cwnd, probe;
	uint32_t current, interval, ts_flush, xmit;
	uint32_t nrcv_buf, nsnd_buf;
	uint32_t nrcv_que, nsnd_que;
	uint32_t nodelay;
	bool updated;
	uint32_t ts_probe, probe_wait;
	uint32_t dead_link, incr;

	SeqQueue snd_queue;
	SeqQueue rcv_queue;
	SeqQueue snd_buf;
	SeqQueue rcv_buf;

	uint32_t* acklist;
	uint32_t ackcount;
	uint32_t ackblock;

	void* user;
	char* buffer;
	int fastresend;
	int fastlimit;
	int nocnwd, stream;
	int logmask;

	OutputFn on_output_{};
	WriteLogFn on_write_log_{};
};

/*
* MTU (Maximum Transmission Unit)
* RTT (Round Trip Time)
* RTO (Retransmission TimeOut)
* 
* 
*/