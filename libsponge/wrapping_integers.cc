#include "wrapping_integers.hh"

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
    return WrappingInt32{isn + static_cast<uint32_t>(n)};
}

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
    
    // the datatype of (n - isn) is int32. When int32 was coverted to uint32_t, uint_max + 1 was added implicitly
    // see the link for more detail: https://stackoverflow.com/questions/4975340/int-to-unsigned-int-conversion
    uint32_t offset = n - isn;

    if (checkpoint > offset){
        // add half of 2^32 in order to round up or round down for the distance between checkpoint and offset
        uint64_t temp = (checkpoint - offset) + (1ul << 31);
        // calculate how many wraps
        uint64_t wrapNum = temp / (1ul << 32);
        return wrapNum * (1ul << 32) + offset;
    }
    else {
        return offset;
    }

}
