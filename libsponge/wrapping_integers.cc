#include "wrapping_integers.hh"

#include <iostream>
#include <cmath>
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint32_t tmp = (n << 32) >> 32;
    return isn + tmp;
}
// 2486501286038614684
// 2486501285533896700
//          1873502398
//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    DUMMY_CODE(n, isn, checkpoint);
    // uint64_t asn = 0;
    // if(n.raw_value() >= isn.raw_value()){
    //     asn = n.raw_value() - isn.raw_value();
    // }else{
    //     asn = (1ul<<32) - isn.raw_value() + n.raw_value();
    // }
    // if((asn + (1ul<<32)) < (1ul<<32)){
    //     return asn;
    // }
    // while (asn + (1ul<<32) <= checkpoint){
    //     asn = asn + (1ul<<32);
    // }
    // if (abs(int64_t(asn + (1ul<<32) - checkpoint)) <= abs(int64_t(checkpoint - asn))){
    //     return asn+(1ul<<32);
    // }
    // return asn;
    uint32_t offset = n.raw_value() - isn.raw_value();
    uint64_t t = (checkpoint & 0xFFFFFFFF00000000) + offset;//原来的 while 累加可能会超时，这个方法是把 checkpoint 的高32位与 offset 相加，直接得到一个相对接近 checkpoint 的值
    uint64_t ret = t;
    if (abs(int64_t(t + (1ul << 32) - checkpoint)) < abs(int64_t(t - checkpoint)))
        ret = t + (1ul << 32);
    if (t >= (1ul << 32) && abs(int64_t(t - (1ul << 32) - checkpoint)) < abs(int64_t(ret - checkpoint)))
        ret = t - (1ul << 32);
    return ret;
}
