#include "kcp.h"

#include <stdarg.h>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <cassert>
#include <cstring>

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

namespace {
	inline int32_t timediff(uint32_t later, uint32_t earlier) {
		return (int32_t)(later - earlier);
	}

	/* encode 8 bits unsigned int */
	inline char* ikcp_encode8u(char* p, unsigned char c)
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
	inline char* ikcp_encode16u(char* p, unsigned short w)
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
	inline const char* ikcp_decode16u(const char* p, unsigned short* w)
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
	inline	char* ikcp_encode32u(char* p, uint32_t l)
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

	inline uint32_t ibound(uint32_t lower, uint32_t middle, uint32_t upper) {
		return std::min(std::max(lower, middle), upper);
	}
}



void Kcp::write_log(int mask, const char* fmt, ...) {

	if ((mask & logmask) == 0 || !on_write_log_)
		return;

	char buffer[1024];
	va_list argptr;
	va_start(argptr, fmt);
	vsprintf(buffer, fmt, argptr);
	va_end(argptr);
	on_write_log_(buffer, this, this->user);
}

Kcp::Kcp(uint32_t conv_, void* user_)
	:conv{ conv_ }, mtu{ IKCP_MTU_DEF }, mss{ mtu - IKCP_OVERHEAD }, state{ 0 },
	snd_una{ 0 }, snd_nxt{ 0 }, rcv_nxt{ 0 },
	ts_recent{ 0 }, ts_lastack{ 0 }, ssthresh{ IKCP_THRESH_INIT },
	rx_rttval{ 0 }, rx_srtt{ 0 }, rx_rto{ IKCP_RTO_DEF }, rx_minrto{ IKCP_RTO_MIN },
	snd_wnd{ IKCP_WND_SND }, rcv_wnd{ IKCP_WND_RCV }, rmt_wnd{ IKCP_WND_RCV }, cwnd{ 0 }, probe{ 0 },
	current{ 0 }, interval{ IKCP_INTERVAL }, ts_flush{ IKCP_INTERVAL }, xmit{ 0 },
	nrcv_buf{ 0 }, nsnd_buf{ 0 },
	nrcv_que{ 0 }, nsnd_que{ 0 },
	nodelay{ 0 },
	updated{ false },
	ts_probe{ 0 }, probe_wait{ 0 },
	dead_link{ IKCP_DEADLINK }, incr{ 0 },
	acklist{ nullptr },
	ackcount{ 0 },
	ackblock{ 0 },
	user{ user_ },
	buffer{ nullptr },
	fastresend{ 0 },
	fastlimit{ IKCP_FASTACK_LIMIT },
	nocnwd{ 0 }, stream{ 0 },
	logmask{ 0 }
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
		delete[] acklist;
}
//---------------------------------------------------------------------
// user/upper level recv: returns size, returns below zero for EAGAIN
//---------------------------------------------------------------------
int Kcp::recv(char* buffer_, int len_) {

	if (rcv_queue.empty())return-1;

	bool ispeek = (len_ < 0) ? true : false;
	bool recover = false;

	if (len_ < 0)len_ = -len_;

	int peeksize_ = peeksize();
	if (peeksize_ < 0) {
		return -2;
	}
	if (peeksize_ > len_) {
		return-3;
	}

	if (nrcv_que >= rcv_wnd) {
		recover = true;
	}

	// merge fragment	
	len_ = 0;
	for (auto it = rcv_queue.begin(); it != rcv_queue.end();) {

		auto seg = *it;
		int frg_;

		if (buffer_) {
			memcpy(buffer_, seg->data, seg->len);
			buffer_ += seg->len;
		}
		len_ += seg->len;
		frg_ = seg->frg;

		if (canlog(log_Recv)) {
			write_log(log_Recv, "recv sn=%lu", seg->sn);
		}

		if (ispeek) {
			delete seg;
			it = rcv_queue.erase(it);
			nrcv_que--;
		}
		else {
			++it;
		}

		if (frg_ == 0)break;
	}

	assert(len_ == peeksize_);

	// move available data from rcv_buf -> rcv_queue
	while (!rcv_buf.empty()) {
		auto seg = rcv_buf.begin();
		if ((*seg)->sn == rcv_nxt && nrcv_que < rcv_wnd) {
			rcv_buf.erase(seg);
			nrcv_buf--;
			rcv_queue.push_back(*seg);
			nrcv_que++;
			rcv_nxt++;
		}
		else {
			break;
		}
	}

	// fast recover
	if (nrcv_que < rcv_wnd && recover) {
		// ready to send back IKCP_CMD_WINS in ikcp_flush
		// tell remote my window size
		probe |= IKCP_ASK_TELL;
	}
	return len_;
}

//---------------------------------------------------------------------
// user/upper level send, returns below zero for error
//---------------------------------------------------------------------
int Kcp::send(const char* buffer_, int len_)
{
	assert(mss > 0);
	if (len_ < 0) { 
		return -1; 
	}

	int sent = 0;
	kcpSeg* seg = nullptr;
	int count = 0;

	// append to previous segment in streaming mode(if possible)
	if (stream != 0) {
		if (!snd_queue.empty()) {			
			auto old = snd_queue.begin();
			if ((*old)->len < mss) {
				int cap = mss - (*old)->len;
				int extend = (len_ < cap) ? len_ : cap;
				seg = new kcpSeg((*old)->len + extend, conv, (*old)->cmd, 0, (*old)->wnd, (*old)->ts, (*old)->una);
				assert(seg);
				if (!seg) {
					return -2;
				}
				snd_queue.push_back(seg);
				memcpy(seg->data, (*old)->data, (*old)->len);
				if (buffer_) {
					memcpy(seg->data + (*old)->len, buffer_, extend);
					buffer_ += extend;
				}
				seg->frg = 0;
				len_ -= extend;
				delete *old;
				snd_queue.erase(old);
				sent = extend;
			}
		}
		if (len_ <= 0)
			return sent;
	}

	if (len_ <= (int)mss) {
		count = 1; 
	}else {
		count = (len_ + mss - 1) / mss;
	}

	if (count >= IKCP_WND_RCV) {
		if (stream != 0 && sent > 0)
			return sent;
		return -2;
	}

	if (count == 0) {
		count = 1;
	}

	// fragment
	for (int i = 0; i < count; ++i) {
		int size = len_ > (int)mss ? (int)mss : len_;
		seg = new kcpSeg(size);
		assert(seg);
		if (!seg) {
			return -2;
		}

		if (buffer_ && len_ > 0) {
			memcpy(seg->data, buffer_, size);
		}
		seg->frg = stream == 0 ? (count - i - 1) : 0;
		snd_queue.push_back(seg);
		nsnd_que++;
		if (buffer_) {
			buffer_ += size;
		}
		len_ -= size;
		sent += size;
	}

	return 0;
}

/* flush */
void Kcp::flush()
{
	if (!updated)return;

	kcpSeg seg{ 0,conv,IKCP_CMD_ACK,0,(uint32_t)wnd_unused(),0,rcv_nxt };

	char* buffer_ = buffer;
	char* ptr_ = buffer;
	auto current_ = current;


	// flush acknowledges
	int count = ackcount;
	int size = 0;
	for (int i = 0; i != count; ++i) {
		size = int(ptr_ - buffer_);
		if (size + IKCP_OVERHEAD > (int)mtu) {
			output(buffer_, size);
			ptr_ = buffer_;
		}
		ack_get(i, &seg.sn, &seg.ts);
		ptr_ = encode_seg(ptr_, &seg);
	}

	ackcount = 0;
	//probe window size(if remote window size equals zero)
	if (rmt_wnd == 0) {
		if (probe_wait == 0) {
			probe_wait = IKCP_PROBE_INIT;
			ts_probe = current + probe_wait;
		}
		else {
			if (timediff(current, ts_probe) >= 0) {
				if (probe_wait < IKCP_PROBE_INIT)
					probe_wait = IKCP_PROBE_INIT;
				probe_wait += probe_wait / 2;
				if (probe_wait > IKCP_PROBE_LIMIT)
					probe_wait = IKCP_PROBE_LIMIT;
				ts_probe = current + probe_wait;
				probe |= IKCP_ASK_SEND;
			}
		}
	}
	else {
		ts_probe = 0;
		probe_wait = 0;
	}

	// flush window probing commands
	if (probe & IKCP_ASK_SEND) {
		seg.cmd = IKCP_CMD_WASK;
		size = (int)(ptr_ - buffer_);
		if (size + (int)IKCP_OVERHEAD > (int)mtu) {
			output(buffer_, size);
			ptr_ = buffer_;
		}
		ptr_ = encode_seg(ptr_, &seg);
	}

	// flush window probing commands
	if (probe & IKCP_ASK_TELL) {
		seg.cmd = IKCP_CMD_WINS;
		size = (int)(ptr_ - buffer_);
		if (size + (int)IKCP_OVERHEAD > (int)mtu) {
			output(buffer_, size);
			ptr_ = buffer_;
		}
		ptr_ = encode_seg(ptr_, &seg);
	}

	probe = 0;
	// calculate window size
	uint32_t cwnd_ = std::min(snd_wnd, rmt_wnd);
	if (nocnwd == 0)
		cwnd_ = std::min(cwnd, cwnd_);

	// calculate resent
	uint32_t resent_, rtomin_;

	// move data from snd_queue to snd_buf
	while (timediff(snd_nxt, snd_una + cwnd_) < 0)
	{
		kcpSeg* newseg = nullptr;

		if (snd_queue.empty())break;

		newseg = snd_queue.front();
		snd_queue.pop_front();
		snd_buf.push_back(newseg);
		nsnd_que--;
		nsnd_buf++;

		newseg->conv = conv;
		newseg->cmd = IKCP_CMD_PUSH;
		newseg->wnd = seg.wnd;
		newseg->ts = current_;
		newseg->sn = snd_nxt++;
		newseg->una = rcv_nxt++;
		newseg->resendts = current_;
		newseg->rto = rx_rto;
		newseg->fastack = 0;
		newseg->xmit = 0;
	}

	// calculate resent
	resent_ = (fastresend > 0) ? fastresend : 0xffffffff;
	rtomin_ = (nodelay == 0) ? (rx_rto >> 3) : 0;

	bool lost = false;
	bool change = false;
	// flush data segments
	for (auto it = snd_buf.begin(); it != snd_buf.end(); ++it) {
		bool needsend = false;
		kcpSeg* segment = *it;
		if (segment->xmit == 0) {
			needsend = true;
			segment->xmit++;
			segment->rto = rx_rto;
			segment->resendts = current_ + segment->rto + rtomin_;
		}
		else if (timediff(current_, segment->resendts) >= 0) {
			needsend = true;
			segment->xmit++;
			xmit++;
			if (nodelay == 0) {
				segment->rto += std::max(segment->rto, (uint32_t)rx_rto);
			}
			else {
				int32_t step = (nodelay < 2) ? (int32_t)segment->rto : rx_rto;
				segment->rto += step / 2;
			}
			segment->resendts = current_ + segment->rto;
			lost = true;
		}
		else if (segment->fastack >= resent_) {
			if ((int)segment->xmit <= fastlimit || fastlimit <= 0) {
				needsend = true;
				segment->xmit++;
				segment->fastack = 0;
				segment->resendts = current_ + segment->rto;
				change = true;
			}
		}

		if (needsend) {
			int need;
			segment->ts = current_;
			segment->wnd = seg.wnd;
			segment->una = rcv_nxt;

			size = (int)(ptr_ - buffer_);
			need = IKCP_OVERHEAD + segment->len;

			if (size + need > (int)mtu) {
				output(buffer_, size);
				ptr_ = buffer_;
			}

			ptr_ = encode_seg(ptr_, segment);
			if (segment->len > 0) {
				memcpy(ptr_, segment->data, segment->len);
				ptr_ += segment->len;
			}

			if (segment->xmit >= dead_link) {
				state = UINT32_MAX;
			}
		}
	}

	// flash remain segments
	size = (int)(ptr_ - buffer_);
	if (size > 0) {
		output(buffer_, size);
	}

	// update ssthresh
	if (change) {
		uint32_t inflight = snd_nxt - snd_una;
		ssthresh = inflight / 2;
		if (ssthresh < IKCP_THRESH_MIN)
			ssthresh = IKCP_THRESH_MIN;
		cwnd = ssthresh + resent_;
		incr = cwnd * mss;
	}

	if (lost) {
		ssthresh = cwnd_ / 2;
		if (ssthresh < IKCP_THRESH_MIN)
			ssthresh = IKCP_THRESH_MIN;
		cwnd = 1;
		incr = mss;
	}

	if (cwnd < 1) {
		cwnd = 1;
		incr = mss;
	}
}


/* input data */
int Kcp::input(const char* data_, long size_) {

	if (canlog(log_Input)) {
		write_log(log_Input, "[RI] %d bytes", size_);
	}

	if (!data_ || size_ < IKCP_OVERHEAD) {
		return -1;
	}

	uint32_t prev_una = snd_una;
	uint32_t maxack = 0, latest_ts = 0;

	bool flag = false;
	while (true) {
		uint32_t ts_, sn_, len_, una_, conv_;
		uint16_t wnd_;
		uint8_t cmd_, frg_;
		kcpSeg* seg = nullptr;

		if (size_ < IKCP_OVERHEAD)break;

		data_ = ikcp_decode32u(data_, &conv_);
		if (conv_ != conv) {
			return-1;
		}

		data_ = ikcp_decode8u(data_, &cmd_);
		data_ = ikcp_decode8u(data_, &frg_);
		data_ = ikcp_decode16u(data_, &wnd_);
		data_ = ikcp_decode32u(data_, &ts_);
		data_ = ikcp_decode32u(data_, &sn_);
		data_ = ikcp_decode32u(data_, &una_);
		data_ = ikcp_decode32u(data_, &len_);

		size_ -= IKCP_OVERHEAD;

		if ((long)size_ < (long)len_ || (int)len_ < 0)return -2;

		if (!valide_cmd(cmd_)) { return-3; }

		rmt_wnd = wnd_;
		parse_una(una_);
		shrink_buf();

		switch (cmd_)
		{
		case IKCP_CMD_ACK:
			if (timediff(current, ts_) >= 0) {
				update_ack(timediff(current, ts_));
			}
			parse_ack(sn_);
			shrink_buf();
			if (!flag) {
				flag = true;
				maxack = sn_;
				latest_ts = ts_;
			}
			else {
				if (timediff(sn_, maxack) > 0) {
#ifndef  IKCP_FASTACK_CONSERVE
					maxack = sn_;
					latest_ts = ts_;
#else
					if (timediff(ts_, latest_ts) > 0) {
						maxack = sn_;
						latest_ts = ts_;
					}
#endif // ! IKCP_FASTACK_CONSERVE
				}
			}
			if (canlog(log_In_Ack)) {
				write_log(log_In_Ack, "input ack: sn=%lu, rtt=%ld,rto=%ld", sn_, timediff(current, ts_), rx_rto);
			}
			break;
		case IKCP_CMD_PUSH:
			if (canlog(log_In_Data)) {
				write_log(log_In_Data, "input psh:sn=%lu ts=%lu", sn_, ts_);
			}
			if (timediff(sn_, rcv_nxt + rcv_wnd) < 0) {
				ack_push(sn_, ts_);
				seg = new kcpSeg(len_, conv, cmd_, frg_, wnd_, ts_, una_);
				if (len_) {
					memcpy(seg->data, data_, len_);
				}
				parse_data(seg);
			}
			break;
		case IKCP_CMD_WASK:
			// ready to send back IKCP_CMD_WINS in ikcp_flush
			// tell remote my window size
			probe |= IKCP_ASK_TELL;
			if (canlog(log_In_Probe)) {
				write_log(log_In_Probe, "input probe");
			}
			break;
		case IKCP_CMD_WINS:
			// do nothing
			if (canlog(log_In_Wins)) {
				write_log(log_In_Wins, "input wins: %lu", (uint32_t)wnd_);
			}
			break;
		default:
			return -3;
		}

		data_ += len_;
		size_ -= len_;
	}

	if (flag) {
		parse_fastack(maxack, latest_ts);
	}

	if (timediff(snd_una, prev_una) > 0) {
		if (cwnd < rmt_wnd) {
			uint32_t mss_ = mss;
			if (cwnd < ssthresh) {
				cwnd++;
				incr += mss_;
			}
			else {
				if (incr < mss_)
					incr = mss_;
				incr += (mss_ * mss_) / incr + (mss_ / 16);
				if ((cwnd + 1) * mss_ <= incr) {
#if 1
					cwnd = (incr + mss_ - 1) / ((mss_ > 0) ? mss_ : 1);
#else
					cwnd++;
#endif
				}
			}

			if (cwnd < rmt_wnd) {
				cwnd = rmt_wnd;
				incr = rmt_wnd * mss_;
			}
		}
	}

	return 0;
}

//---------------------------------------------------------------------
// update state (call it repeatedly, every 10ms-100ms), or you can ask 
// ikcp_check when to call it again (without ikcp_input/_send calling).
// 'current' - current timestamp in millisec. 
//---------------------------------------------------------------------
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

bool Kcp::valide_cmd(uint8_t cmd_)
{
	return cmd_ == IKCP_CMD_PUSH ||
		cmd_ == IKCP_CMD_ACK ||
		cmd_ == IKCP_CMD_WASK ||
		cmd_ == IKCP_CMD_WINS;
}

uint32_t Kcp::check(uint32_t current_) {

	if (!updated) {
		return current_;
	}

	uint32_t ts_flush_ = ts_flush;

	if (timediff(current_, ts_flush_) >= 10000 ||
		timediff(current_, ts_flush_) < -10000) {
		ts_flush_ = current_;
	}

	if (timediff(current_, ts_flush_) >= 0) {
		return current_;
	}

	int32_t tm_packet_ = 0x7fffffff;
	int32_t tm_flush_ = timediff(ts_flush_, current_);

	for (auto it = snd_buf.begin(); it != snd_buf.end(); ++it) {

		int32_t diff = timediff((*it)->resendts, current_);
		if (diff <= 0)
			return current_;
		if (diff < tm_packet_)
			tm_packet_ = diff;
	}

	uint32_t minimal = (uint32_t)(std::min(tm_packet_, tm_flush_));
	if (minimal >= interval)
		minimal = interval;

	return current_ + minimal;
}

/* peek data size */
int Kcp::peeksize()
{
	if (rcv_queue.empty())
		return -1;

	auto p = rcv_queue.begin();
	if ((*p)->frg == 0)return (*p)->len;

	if (nrcv_que < (*p)->frg + 1)return -1;

	int len_ = 0;
	for (; p != rcv_queue.end(); ++p) {
		len_ += (*p)->len;
		if ((*p)->frg == 0)break;
	}
	return len_;
}

int Kcp::setmtu(int mtu_)
{
	if (mtu_ < 50 || mtu_ < IKCP_OVERHEAD)
		return -1;

	char* buffer_ = new char[(mtu_ + IKCP_OVERHEAD) * 3];
	if (!buffer_)
		return -2;
	mtu = mtu_;
	mss = mtu - IKCP_OVERHEAD;
	delete[] buffer;
	buffer = buffer_;
	return 0;
}

int Kcp::setinterval(int interval_)
{
	if (interval_ > 5000)
		interval_ = 5000;
	else if (interval_ < 10)
		interval_ = 10;
	interval = interval_;
	return 0;
}

int Kcp::set_nodelay(int nodelay_, int interval_, int resend_, int nc_) {

	if (nodelay_ >= 0) {
		nodelay = nodelay_;
		if (nodelay_) {
			rx_minrto = IKCP_RTO_NDL;
		}
		else {
			rx_minrto = IKCP_RTO_MIN;
		}
	}
	if (interval_ > 0) {
		setinterval(interval_);
	}
	if (resend_ >= 0) {
		fastresend = resend_;
	}
	if (nc_ >= 0) {
		nocnwd = nc_;
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

char* Kcp::encode_seg(char* ptr_, const kcpSeg* seg_)
{
	ptr_ = ikcp_encode32u(ptr_, seg_->conv);
	ptr_ = ikcp_encode8u(ptr_, (uint8_t)seg_->cmd);
	ptr_ = ikcp_encode8u(ptr_, (uint8_t)seg_->frg);
	ptr_ = ikcp_encode16u(ptr_, (uint16_t)seg_->wnd);
	ptr_ = ikcp_encode32u(ptr_, seg_->ts);
	ptr_ = ikcp_encode32u(ptr_, seg_->sn);
	ptr_ = ikcp_encode32u(ptr_, seg_->una);
	ptr_ = ikcp_encode32u(ptr_, seg_->len);
	return ptr_;
}

int Kcp::wnd_unused()
{
	if (nrcv_que < rcv_wnd)
		return rcv_wnd - nrcv_que;
	return 0;
}

int Kcp::output(const char* data, int size) {

	if (canlog(log_Output)) {
		write_log(log_Output, "[RO] %ld bytes", size);
	}
	if (size == 0)return 0;
	return on_output_(data, size, this, user);
}
//---------------------------------------------------------------------
// ack append
//---------------------------------------------------------------------
void Kcp::ack_push(uint32_t sn_, uint32_t ts_)
{
	uint32_t newsize = ackcount + 1;
	if (newsize > ackblock) {
		uint32_t* acklist_;
		uint32_t newblock_;

		for (newblock_ = 8; newblock_ < newsize; newblock_ <<= 1);
		acklist_ = new uint32_t[newblock_ * 2];

		if (!acklist_) {
			throw std::runtime_error("acklist allocator failed");
		}

		if (acklist != nullptr) {
			for (uint32_t i = 0; i < ackcount; ++i) {
				acklist_[i * 2] = acklist[i * 2];
				acklist_[i * 2 + 1] = acklist[i * 2 + 1];
			}
			delete[] acklist;
		}
		acklist = acklist_;
		ackblock = newblock_;
	}

	uint32_t* ptr = &acklist[ackcount * 2];
	ptr[0] = sn_;
	ptr[1] = ts_;
	ackcount++;
}

void Kcp::ack_get(int p, uint32_t* sn_, uint32_t* ts_)
{
	if (sn_)
		sn_[0] = acklist[p * 2];
	if (ts_)
		ts_[0] = acklist[p * 2 + 1];
}

//---------------------------------------------------------------------
// parse data
//---------------------------------------------------------------------
void Kcp::parse_data(kcpSeg* newseg_)
{
	uint32_t sn_ = newseg_->sn;
	if (timediff(sn_, rcv_nxt + rcv_wnd) >= 0 ||
		timediff(sn_, rcv_nxt) < 0)
	{
		delete newseg_;
		return;
	}

	for (auto it = rcv_buf.begin(); it != rcv_buf.end(); ++it) {
		if ((*it)->sn == sn_) {
			delete newseg_;
			break;
		}
		if (timediff(sn_, (*it)->sn) > 0) {
			rcv_buf.insert(it, newseg_);
			nrcv_buf++;
			break;
		}
	}

#if 0
	ikcp_qprint("rcvbuf", &kcp->rcv_buf);
	printf("rcv_nxt=%lu\n", kcp->rcv_nxt);
#endif

	// move available data from rcv_buf -> rcv_queue
	while (!rcv_buf.empty()) {
		auto seg = rcv_buf.begin();
		if ((*seg)->sn == rcv_nxt && nrcv_que < rcv_wnd) {
			rcv_buf.erase(seg);
			nrcv_buf--;
			rcv_queue.push_back(*seg);
			nrcv_que++;
			rcv_nxt++;
		}
		else {
			break;
		}
	}
}

void Kcp::update_ack(int32_t rtt_)
{
	int32_t rto_ = 0;
	if (rx_srtt == 0) {
		rx_srtt = rtt_;
		rx_rttval = rtt_ / 2;
	}
	else {
		long delta = rtt_ - rx_srtt;
		if (delta < 0) delta = -delta;
		rx_rttval = (3 * rx_rttval + delta) / 4;
		rx_srtt = (7 * rx_srtt + rtt_) / 8;
		if (rx_srtt < 1) rx_srtt = 1;
	}
	rto_ = rx_srtt + std::max((int32_t)interval, 4 * rx_rttval);
	rx_rto = ibound(rx_minrto, rto_, IKCP_RTO_MAX);
}

void Kcp::shrink_buf()
{
	if (!snd_buf.empty()) {
		snd_una = snd_buf.front()->sn;
	}
	else {
		snd_una = snd_nxt;
	}
}

void Kcp::parse_ack(uint32_t sn_)
{
	if (timediff(sn_, snd_una) < 0 || timediff(sn_, snd_nxt) >= 0)
		return;

	for (auto it = snd_buf.begin(); it != snd_buf.end(); ++it) {
		if (sn_ == (*it)->sn) {
			delete (*it);
			it = snd_buf.erase(it);
			nsnd_buf--;
			break;
		}
		if (timediff(sn_, (*it)->sn) < 0) {
			break;
		}
	}
}

void Kcp::parse_una(uint32_t una_)
{
	for (auto it = snd_buf.begin(); it != snd_buf.end(); ) {
		if (timediff(una_, (*it)->sn) > 0) {
			delete (*it);
			it = snd_buf.erase(it);
			nsnd_buf--;
		}
		else {
			++it;
		}
	}
}

void Kcp::parse_fastack(uint32_t sn_, uint32_t ts_)
{
	if (timediff(sn_, snd_una) < 0 || timediff(sn_, snd_nxt) >= 0)
		return;

	for (auto it = snd_buf.begin(); it != snd_buf.end(); ++it) {

		if (timediff(sn_, (*it)->sn) < 0) {
			break;
		}
		else if (sn_ != (*it)->sn) {
#ifndef IKCP_FASTACK_CONSERVE
			(*it)->fastack++;
#else
			if (timediff(ts_, (*it)->ts) >= 0) {
				(*it)->fastack++;
			}
#endif
		}
	}
}
