#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    DUMMY_CODE(seg);
    static size_t abs_seqno = 0;
    size_t length = 0;
    bool  ret_flag = false;
    if (seg.header().syn == true){
        if (_syn_flag == false ){
            _syn_flag = true;
            _isn = seg.header().seqno.raw_value();
            abs_seqno = 1;
            _base = 1;
            length = seg.length_in_sequence_space();
            ret_flag = true;
        }
    }
    else if(_syn_flag == false){
        return;
    }
    else{
        abs_seqno = unwrap(seg.header().seqno,WrappingInt32(_isn),abs_seqno);
        length = seg.length_in_sequence_space();
    }

    if (seg.header().fin == true){
        if(_fin_flag == false){
            _fin_flag = true;
            ret_flag = true;
        }
    }
    else if (seg.length_in_sequence_space() == 0 ){
        return;
    }
    else if (abs_seqno >= _base + window_size() || abs_seqno + length <= _base){
        if(ret_flag == false){
            return;
        }
    }
    _reassembler.push_substring(seg.payload().copy(), abs_seqno - 1, seg.header().fin);
    _base = _reassembler.head_index() + 1;
    if (_reassembler.input_ended())  // FIN be count as one byte
        _base++;
    return ;

    

}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (_base > 0)
        return WrappingInt32(wrap(_base, WrappingInt32(_isn)));
    else
        return std::nullopt; }

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
