#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // determine if it is SYN segment
    if( !_synFlag){
        // wait for the SYN segment to come in
        if(seg.header().syn){
            _synFlag = true;
            _isn = seg.header().seqno;
        }
        else return;
    }
    
    uint64_t checkpoint = _reassembler.stream_out().bytes_written() - 1;
    uint64_t abSeqNum = unwrap( seg.header().seqno, _isn, checkpoint);
    // segment with SYN and without data, streamIndex will start from 0 instead of -1
    // segment with SYN and with data, streamIndex will also start from 0 instead of -1
    // segment with only data, streamIndex will start from abSeqNum -1;
    uint64_t streamIndex = abSeqNum - 1 + seg.header().syn;
  
    _reassembler.push_substring(seg.payload().copy(), streamIndex, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    // To determine if we are at Listen Stage.
    // If we are at Listen Stage
    if(!_synFlag) {
        return nullopt;
    }
    
    // If we are at non-Listen Stage, we need to add 1 to get the absolute ack number
    uint64_t abAckNum = _reassembler.stream_out().bytes_written() + 1;
        
    // If we are at FIN_RECV, we need to add 1 more (due to FIN), but we  only do this when stream_out().input_ended() is true
    if(_reassembler.stream_out().input_ended()){
        ++abAckNum;
    }
    
    return wrap(abAckNum, _isn);
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }


