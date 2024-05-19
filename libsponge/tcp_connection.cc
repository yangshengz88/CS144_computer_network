#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    _time_since_last_segment_received = 0;

    if (seg.header().rst){
        _set_rst_state(false);
        return;
    }

    _receiver.segment_received(seg);

    if (seg.header().ack){
        _sender.ack_received(seg.header().ackno, seg.header().win);
        
    }
    
    //at LISTEN state, we receive SYN
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV && 
    TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED){
        connect();
        return;
    }

    //at Close-Wait state
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
    TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED){
        _linger_after_streams_finish = false;
    }

    //Passive Close 
    if (!_linger_after_streams_finish && TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
    TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED){
        _is_active = false;
        return;
    }

    //keep live
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0)
    && seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }


    // if seg.length_in_sequence_space() is > 0 ( we need to send ack), if _segments_out is not empty, we dont need to send an empty_segment for ack purpose, because _segments_out will do us a favor
    // but if _segments_out is empty, we have to send an empty_segment because _segments_out can't do us a favor
    if (seg.length_in_sequence_space() > 0 && _segments_out.empty()){
        _sender.send_empty_segment();
    }

    _add_ackno_and_win_to_send();

}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    auto write_size = _sender.stream_in().write(data);
    _sender.fill_window();
    _add_ackno_and_win_to_send();
    return write_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;

    //call _sender.tick()
    _sender.tick(ms_since_last_tick);
    
    // consecutive retransmissions is bigger than the max, send reset segment
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS){
        while( !_sender.segments_out().empty()) _sender.segments_out().pop();
        _set_rst_state(true);
        return;
    }
     
    //The moment we call TCPConnection::tick(), there may be some new segments that need to retransmit. We don't want to miss that.
    _add_ackno_and_win_to_send();

    if (_linger_after_streams_finish && TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && 
    TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
    _time_since_last_segment_received >= 10 * _cfg.rt_timeout){
        _is_active = false;
        _linger_after_streams_finish = false;
     }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _add_ackno_and_win_to_send();
}
void TCPConnection::connect() {
    _sender.fill_window();
    _add_ackno_and_win_to_send();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            _set_rst_state(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_set_rst_state(bool send_rst){
    if(send_rst){
        TCPSegment rst_seg;
        rst_seg.header().rst = true;
        rst_seg.header().seqno = _sender.next_seqno();
        _segments_out.push(std::move(rst_seg));
    }
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _linger_after_streams_finish = false;
    _is_active = false;
}

void TCPConnection::_add_ackno_and_win_to_send(){
    while(! _sender.segments_out().empty()){
        TCPSegment seg =  _sender.segments_out().front();
        _sender.segments_out().pop();
        if(_receiver.ackno().has_value()){
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(std::move(seg));
    }
}
