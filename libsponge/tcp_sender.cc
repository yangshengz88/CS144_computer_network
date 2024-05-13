#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

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
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout) {}

size_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window(){
    uint16_t window_size = max(_window_size, static_cast<uint16_t>(1));
    while (_bytes_in_flight < window_size){
        
        TCPSegment seg;

        //Firstly, deal with SYN part. The inital window size is 1, so the 1st segment only contain SYN without payload 
        if (! _set_syn_flag){
            seg.header().syn = true;
            _set_syn_flag = true;
        }

        //Secondly, deal with payload part
        auto payload_size = min(min( _stream.buffer_size(), TCPConfig::MAX_PAYLOAD_SIZE), window_size - _bytes_in_flight - seg.header().syn);
        auto payload = _stream.read(payload_size);
        seg.payload() = Buffer(std::move(payload));

        //Thirdly, deal with FIN part
        if( !_set_fin_flag && _stream.eof() && _bytes_in_flight + seg.length_in_sequence_space() <  window_size){
            seg.header().fin = true;
            _set_fin_flag = true;
        }


        // if length is 0, we don't send it out
        uint64_t length = seg.length_in_sequence_space();
        if (length == 0) break;


        // Fourthly, Add the seqno to the header. 
        seg.header().seqno = next_seqno();
        //send out the segment
        _segments_out.push(seg);
        // if the timer is not running, restart it
        if( !_timer.is_running()) _timer.restart();
        //put the segment into the  outstanding_seg queue using the std::move to transfer the ownership.
        _outstanding_seg.emplace(_next_seqno, std::move(seg));

     
        // Lastly, update the byte in flight and next seqno
        _next_seqno += length;
        _bytes_in_flight += length;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto abs_ackno = unwrap(ackno, _isn, _next_seqno);

    // edge case: if abs ackno > absolute next seqno, the segment needs to be discarded, so we don't process the ack_received info.
    if (abs_ackno > _next_seqno) return;
    
    // else: abs ackno <= absolute next seqno
    bool is_successful = 0;
    // as for any outstanding segment whose absolute seqno is <= abs ackno, we need to pop them out, 'cause we have received ACK already.
    while (! _outstanding_seg.empty()){
        // this is structured binding syntax since c++ 17
        auto &[seqno, seg] = _outstanding_seg.front();
        if ((seqno + seg.length_in_sequence_space()) <= abs_ackno){

            is_successful = 1;
            _outstanding_seg.pop();
            _bytes_in_flight -= seg.length_in_sequence_space();
        }
        else break;
    }


    // if ACK is received and we pop the segment out the outstanding_seg successfully, we can reset the timer
    if (is_successful){
        _timer.set_time_out(_initial_retransmission_timeout);
        _timer.restart();
        _consecutive_retransmissions_count = 0;
    }

    //if outstanding_seg is empty, we shut down the timer.
    if( _outstanding_seg.empty()){
        _timer.stop();
    }

    //update the window size and continute to fill in the window
    _window_size = window_size;
    fill_window();

}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick(ms_since_last_tick);
    // if timer ran out of the  fixed timeout, retransmit the oldest segment
    if (_timer.check_time_out() && !_outstanding_seg.empty()){
        
        _segments_out.push(_outstanding_seg.front().second);
        // based on the document, only when the window size is not 0, we can retransmit the segment
        if (_window_size > 0){
            ++_consecutive_retransmissions_count;
            _timer.set_time_out(_timer.get_time_out() * 2);
        }
        _timer.restart();

    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    //send empty segment for ACK
    TCPSegment seg;
    //No SYN, FIN,Payload parts, only seqno part
    seg.header().seqno = next_seqno();
    _segments_out.push(std::move(seg));

}