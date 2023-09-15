#include "kcp.h"

#include <stdarg.h>
#include <stdexcept>

//=====================================================================
// KCP BASIC
//=====================================================================
constexpr auto IKCP_RTO_NDL = 30;		// no delay min rto
constexpr auto IKCP_RTO_MIN = 100;		// normal min rto
constexpr auto IKCP_RTO_DEF = 200;
constexpr auto IKCP_RTO_MAX = 60000;
constexpr auto IKCP_CMD_PUSH = 81;		// cmd: push data
constexpr auto IKCP_CMD_ACK = 82;		// cmd: ack
constexpr auto IKCP_CMD_WASK = 83;		// cmd: window probe (ask)
constexpr auto IKCP_CMD_WINS = 84;		// cmd: window size (tell)
constexpr auto IKCP_ASK_SEND = 1;		// need to send IKCP_CMD_WASK
constexpr auto IKCP_ASK_TELL = 2;		// need to send IKCP_CMD_WINS
constexpr auto IKCP_WND_SND = 32;
constexpr auto IKCP_WND_RCV = 128;       // must >= max fragment size
constexpr auto IKCP_MTU_DEF = 1400;
constexpr auto IKCP_ACK_FAST = 3;
constexpr auto IKCP_INTERVAL = 100;
constexpr auto IKCP_OVERHEAD = 24;
constexpr auto IKCP_DEADLINK = 20;
constexpr auto IKCP_THRESH_INIT = 2;
constexpr auto IKCP_THRESH_MIN = 2;
constexpr auto IKCP_PROBE_INIT = 7000;		// 7 secs to probe window size
constexpr auto IKCP_PROBE_LIMIT = 120000;	// up to 120 secs to probe window
constexpr auto IKCP_FASTACK_LIMIT = 5;		// max times to trigger fastack

int32_t timediff(uint32_t later, uint32_t earlier) {
	return (int32_t)(later - earlier);
}


/* encode 8 bits unsigned int */
char* ikcp_encode8u(char* p, unsigned char c)
{
	*(unsigned char*)p++ = c;
	return p;
}

/* decode 8 bits unsigned int */
const char* ikcp_decode8u(const char* p, unsigned char* c)
{
	*c = *(unsigned char*)p++;
	return p;
}

/* encode 16 bits unsigned int (lsb) */
char* ikcp_encode16u(char* p, unsigned short w)
{
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
	* (unsigned char*)(p + 0) = (w & 255);
	*(unsigned char*)(p + 1) = (w >> 8);
#else
	memcpy(p, &w, 2);
#endif
	p += 2;
	return p;
}

/* decode 16 bits unsigned int (lsb) */
const char* ikcp_decode16u(const char* p, unsigned short* w)
{
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
	* w = *(const unsigned char*)(p + 1);
	*w = *(const unsigned char*)(p + 0) + (*w << 8);
#else
	memcpy(w, p, 2);
#endif
	p += 2;
	return p;
}

/* encode 32 bits unsigned int (lsb) */
char* ikcp_encode32u(char* p, uint32_t l)
{
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
	* (unsigned char*)(p + 0) = (unsigned char)((l >> 0) & 0xff);
	*(unsigned char*)(p + 1) = (unsigned char)((l >> 8) & 0xff);
	*(unsigned char*)(p + 2) = (unsigned char)((l >> 16) & 0xff);
	*(unsigned char*)(p + 3) = (unsigned char)((l >> 24) & 0xff);
#else
	memcpy(p, &l, 4);
#endif
	p += 4;
	return p;
}

/* decode 32 bits unsigned int (lsb) */
inline const char* ikcp_decode32u(const char* p, uint32_t* l)
{
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
	* l = *(const unsigned char*)(p + 3);
	*l = *(const unsigned char*)(p + 2) + (*l << 8);
	*l = *(const unsigned char*)(p + 1) + (*l << 8);
	*l = *(const unsigned char*)(p + 0) + (*l << 8);
#else 
	memcpy(l, p, 4);
#endif
	p += 4;
	return p;
}


void Kcp::log(int mask, const char* fmt, ...) {
	if ((mask & logmask) == 0 || !on_write_log_)
		return;

	char buff[1024];
	va_list argptr;
	va_start(argptr, fmt);
	vsprintf(buffer, fmt, argptr);
	va_end(argptr);
	on_write_log_(buffer, this, this->user);
}

Kcp::Kcp(uint32_t conv_, void* user_)
	:conv{ conv_ }, user{ user_ },
	snd_una{ 0 }, snd_nxt{ 0 }, rcv_nxt{ 0 },
	ts_recent{ 0 }, ts_lastack{ 0 },
	ts_probe{ 0 }, probe_wait{ 0 }, snd_wnd{ IKCP_WND_SND }, rcv_wnd{ IKCP_WND_RCV }, rmt_wnd{ IKCP_WND_RCV },
	cwnd{ 0 }, incr{ 0 }, probe{ 0 }, mtu{ IKCP_MTU_DEF }, mss{ mtu - IKCP_OVERHEAD }, stream{ 0 },
	buffer{ nullptr },
	nrcv_buf{ 0 }, nsnd_buf{ 0 }, nrcv_que{ 0 }, nsnd_que{ 0 }, state{ 0 }, acklist{ nullptr },
	ackblock{ 0 }, ackcount{ 0 }, rx_srtt{ 0 }, rx_rttval{ 0 }, rx_rto{ IKCP_RTO_DEF },
	rx_minrto{ IKCP_RTO_MIN }, current{ 0 }, interval{ IKCP_INTERVAL },
	ts_flush{ IKCP_INTERVAL }, nodelay{ 0 }, updated{ false }, logmask{ 0 }, ssthresh{ IKCP_THRESH_INIT },
	fastresend{ 0 }, fastlimit{ IKCP_FASTACK_LIMIT }, nocnwd{ 0 }, xmit{ 0 }, dead_link{ IKCP_DEADLINK }
{
	buffer = new char[mtu + IKCP_OVERHEAD * 3];
	if (!buffer) {
		throw std::runtime_error("Kcp Buffer allocater failed");
	}
}

Kcp::~Kcp()
{
	while (!snd_buf.empty()) {
		auto seg = snd_buf.front();
		snd_buf.pop_front();
		delete seg;
	}
	while (!rcv_buf.empty()) {
		auto seg = rcv_buf.front();
		rcv_buf.pop_front();
		delete seg;
	}
	while (!snd_queue.empty()) {
		auto seg = snd_queue.front();
		snd_queue.pop_front();
		delete seg;
	}
	while (!rcv_queue.empty()) {
		auto seg = rcv_queue.front();
		rcv_queue.pop_front();
		delete seg;
	}

	if (buffer)
		delete[] buffer;
	if (acklist)
		delete acklist;
}

int Kcp::recv(char* buffer, int len) {

	if (rcv_queue.empty())return-1;

	bool ispeek = (len < 0) ? true : false;
	int recover = 0;

	if (len < 0)len = -len;

	int peeksize = get_peeksize();
	if (peeksize < 0)
		return -2;
	if (peeksize > len)
		return-3;

	if (nrcv_que >= rcv_wnd)
		recover = 1;

	// merge fragment
	len = 0;




}

void Kcp::flush()
{

}

int Kcp::input(const char* data, long size) {

	if (canlog(log_Input)) {
		log(log_Input, "[RI] %d bytes", size);
	}

	if (!data || size < IKCP_OVERHEAD)
		return -1;

}

void Kcp::update(uint32_t current_) {
	current = current_;

	if (!updated) {
		updated = true;
		ts_flush = current;
	}

	int32_t slap = timediff(current, ts_flush);
	if (slap >= 10000 || slap < -10000) {
		ts_flush = current;
		slap = 0;
	}

	if (slap >= 0) {
		ts_flush += interval;
		if (timediff(current, ts_flush) >= 0) {
			ts_flush = current + interval;
		}
		flush();
	}
}

uint32_t Kcp::check(uint32_t current) {

	if (!updated)
		return current;
	uint32_t ts_flush_ = ts_flush;

	if (timediff(current, ts_flush_) >= 10000 ||
		timediff(current, ts_flush_ < -10000)) {
		ts_flush_ = current;
	}

	if (timediff(current, ts_flush_) >= 0) {
		return current;
	}

	int32_t tm_packet = 0x7fffffff;
	int32_t tm_flush = timediff(ts_flush_, current);

	for (auto it = snd_buf.begin(); it != snd_buf.end(); ++it) {

		int32_t diff = timediff((*it)->resendts, current);
		if (diff <= 0)
			return current;
		if (diff < tm_packet)
			tm_packet = diff;
	}

	uint32_t minimal = (uint32_t)(std::min(tm_packet, tm_flush));
	if (minimal >= interval)
		minimal = interval;

	return current + minimal;
}

int Kcp::get_peeksize()
{

}

int Kcp::setmtu(int mtu_) {
	if (mtu_ < 50 || mtu_ < IKCP_OVERHEAD)
		return -1;
	char* buffer_ = new char[mtu_ + IKCP_OVERHEAD * 3];
	if (!buffer_)
		return -2;
	mtu = mtu_;
	mss = mtu - IKCP_OVERHEAD;
	delete[] buffer;
	buffer = buffer_;
	return 0;
}

int Kcp::setinterval(int interval_) {
	if (interval_ > 5000)
		interval_ = 5000;
	else if (interval_ < 10)
		interval_ = 10;
	interval = interval_;
}

int Kcp::set_nodelay(int nodelay_, int interval_, int resend_, int nc) {

	if (nodelay_ >= 0) {
		nodelay = nodelay_;
		if (nodelay) {
			rx_minrto = IKCP_RTO_NDL;
		}
		else {
			rx_minrto = IKCP_RTO_MIN;
		}
	}
	setinterval(interval_);
	if (resend_ >= 0) {
		fastresend = resend_;
	}
	if (nc >= 0) {
		nocnwd = nc;
	}
	return 0;
}

int  Kcp::set_wndsize(int sndwnd_, int rcvwnd_) {
	if (sndwnd_ > 0) {
		snd_wnd = sndwnd_;
	}
	if (rcvwnd_ > 0) {// must >= max fragment size
		rcv_wnd = std::max(rcvwnd_, IKCP_WND_RCV);
	}
	return 0;
}

bool Kcp::canlog(int mask) {
	return ((mask & logmask) != 0) && on_write_log_ != nullptr;
}

int Kcp::waitsnd() {
	return nsnd_buf + nsnd_que;
}