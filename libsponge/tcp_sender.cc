#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>
#define MAX_RESEND_TIMES 8ul

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    // : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _recent_abs_ackno{0}
    , _initial_retransmission_timeout{retx_timeout}
    , _retransmission_timeout{_initial_retransmission_timeout}
    , _time_remain{0}
    , _stream(capacity) 
    , _window_size(1) 
    , _bytes_in_flight(0) {
        cout << "make sender" << endl;
    }

uint64_t TCPSender::bytes_in_flight() const {cout << "call flying: " << _bytes_in_flight << endl; return _bytes_in_flight; }

void TCPSender::fill_window() {
    // if(_fin_flag){
    //     cout << "_fin_return" << endl;
    //     return;
    // }
    cout << "call fill" << endl;
    size_t length_remain = 0;
    uint64_t old_seqno = next_seqno_absolute();
    //SYN
    if(_syn_flag == false){
        TCPSegment seg = TCPSegment();
        seg.header().syn = true;
        seg.header().seqno = wrap(_next_seqno,_isn);
        cout << "send: SYN"  << endl;
        _segments_out.push(seg);
        _segments_outstanding.push(seg);
        _seqno_not_ack.push(next_seqno_absolute());
        _bytes_in_flight += 1;
        _r += 1;
        _next_seqno += 1;
        _syn_flag = true;
        _fin_flag = false;
        if(_timer_running == false){
            _timer_running = true;
            _time_remain = 0;
        }
        return;
    }
    if (stream_in().buffer_size() == 0 || stream_in().eof()){
        printf(">>>eof\n");
        if(_stream.input_ended() && !_fin_flag){
            cout << "win: " << _window_size << "flying: " << _bytes_in_flight << endl;
            if(_window_size > static_cast<size_t>(_bytes_in_flight)){
                cout << "_window_size - _bytes_in_flight: " << _window_size - _bytes_in_flight << endl;
                TCPSegment seg = TCPSegment();
                seg.header().fin = true;
                seg.header().seqno = next_seqno();
                _seqno_not_ack.push(_next_seqno);
                _next_seqno += seg.length_in_sequence_space();
                _segments_out.push(seg);
                _segments_outstanding.push(seg);
                _bytes_in_flight += 1;
                _fin_flag = true;
                cout << "sent: FIN"  << endl;
                if(_timer_running == false){
                    _timer_running = true;
                    _time_remain = 0;
                }
            }
        }
        return;
    }else{
        cout << stream_in().buffer_size() << endl;
    }
    //
    length_remain = min(static_cast<size_t>(_window_size) - _bytes_in_flight, TCPConfig::MAX_PAYLOAD_SIZE);
    while (length_remain > 0 && !_stream.buffer_empty() && _window_size > 0){
        TCPSegment seg = TCPSegment();
        length_remain = min(length_remain, stream_in().buffer_size());
        cout << "length_remain: "<< length_remain << endl;
        cout << "window_size: "<< static_cast<size_t>(_window_size) << endl;
        string str = stream_in().read(length_remain);
        seg.payload() = Buffer(move(str));
        //_window_size -= seg.length_in_sequence_space();
        _bytes_in_flight += seg.payload().copy().length();
        cout << "seg len before set fin: "<< seg.length_in_sequence_space() << endl;
        //  || seg.length_in_sequence_space() < length_remain
        if ((_stream.input_ended()) && !_fin_flag){
            if((_window_size - _bytes_in_flight > 0) /*&& TCPConfig::MAX_PAYLOAD_SIZE - _bytes_in_flight > 0*/){
                seg.header().fin = true;
                _fin_flag = true;
                
                cout << "set: FIN"  << endl;  
                _bytes_in_flight++;
            }
        }
        //set seqno
        old_seqno = next_seqno_absolute();
        seg.header().seqno = next_seqno();
        _next_seqno += seg.length_in_sequence_space();
        
        cout << "flying: "<< bytes_in_flight() << endl;
        cout << "seg len: "<< seg.length_in_sequence_space() << endl;
        _r += seg.length_in_sequence_space();
        // cout << "send: " << seg.payload().copy() << endl;
        _segments_out.push(seg);
        _segments_outstanding.push(seg);
        _seqno_not_ack.push(old_seqno);
        // cout << "sent: " << _segments_out.back().payload().copy() << endl;
        if(_window_size - _bytes_in_flight  <= 0){
            break;
        }
    }
    //start timer
    if(_timer_running == false){
        _timer_running = true;
        _time_remain = 0;
    }
    printf(">>>fill ok\n");
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    cout << "call ack"<< endl;
    _win_zero_flag = false;
    uint64_t ack_abs = unwrap(ackno, _isn, _next_seqno);
    _window_size = window_size;
    if(window_size == 0){
        _win_zero_flag = true;
        _window_size = 1;
    }
    if (ack_abs > _next_seqno || ack_abs < _recent_abs_ackno){
        cout << "??error??" << endl;
        cout << ">>>ack_abs " << ack_abs << " but next_seqno " << _next_seqno <<  endl;
        return;

    }
    _l = max(ack_abs+1,_l);
    _recent_abs_ackno = ack_abs;
    if(_timer_running == false){
        _timer_running = true;
        _retransmission_timeout = _initial_retransmission_timeout;
        _resend_times = 0;
    }
    while (!_segments_outstanding.empty() /*&& !_win_zero_flag*/) {
        TCPSegment seg = _segments_outstanding.front();
        uint64_t seqno = unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space();
        cout << "ack_abs: " << ack_abs << "seqno: " << seqno << endl;
        if (seqno <= ack_abs) {
            _recent_abs_ackno = seqno;
            _segments_outstanding.pop();
            _bytes_in_flight -= seg.payload().copy().length();
            if(seg.header().syn){
                _bytes_in_flight--;
            }
            else if(seg.header().fin){
                _bytes_in_flight --;
                _fin_acked = true;
            }
            cout << "flying: "<< bytes_in_flight() << endl;
            cout << "poped: "<< seg.payload().copy()<< endl;
            // restart timer
            _time_remain= 0;
            _resend_times = 0;
            _retransmission_timeout = _initial_retransmission_timeout;
        }else{
            break;
        }
        
    }
    if (_segments_outstanding.empty() && !_win_zero_flag) {
        // restart timer
        _time_remain= 0;
        _retransmission_timeout = _initial_retransmission_timeout;
        _timer_running = false;
        _resend_times = 0;
    }
    //如果没有发送但未确认的报文，关闭计时器
    if (bytes_in_flight() == 0) {
        _timer_running = false;
    }else{
        _timer_running = true;
    }
    if (bytes_in_flight() > static_cast<uint64_t>(_window_size)) {
        cout << "??window too small?" << endl;
        cout << "bif is " << _bytes_in_flight << " window size is " << _window_size  << endl;
        return;
    }

    //fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    //DUMMY_CODE(ms_since_last_tick); 
    cout << "call tick" << endl;
    if(_timer_running == false){
        cout << "tick not running" << endl;
        return;
    }
    _time_remain += ms_since_last_tick;
    //resend
    if(_time_remain >= _retransmission_timeout){
        _time_remain = 0;
        if(!_segments_outstanding.empty()){
            _segments_out.push(_segments_outstanding.front());
            cout << "resent: " << _segments_outstanding.front().payload().copy() << endl;
        }else{
            cout << "<ERROR> timmer should not running" << endl;
            return;
        }

        _resend_times++;
        _time_remain = 0;
        if(!_win_zero_flag){
            _retransmission_timeout = 2 * _retransmission_timeout;
            
        }

    }
    
}

unsigned int TCPSender::consecutive_retransmissions() const {cout << "call retend_times: " << _resend_times << endl; return {_resend_times}; }

void TCPSender::send_empty_segment() {
    cout << "send empty" << endl;
    TCPSegment seg = TCPSegment();
    // size_t length = TCPConfig::MAX_PAYLOAD_SIZE;
    // size_t length_seg=0, length_remain = 0;
    // uint64_t seqno;
    seg.header().seqno = next_seqno();
    _segments_outstanding.push(seg);
    
}
