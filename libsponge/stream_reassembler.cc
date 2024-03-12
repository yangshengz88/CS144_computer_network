#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _stream(capacity), _expected_index(0), _eof_index(numeric_limits<size_t>::max()), _unassemble_byte_cnt(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t start = max(index, _expected_index);
    size_t end = min(index + data.size(), _expected_index + (_capacity - _output.buffer_size()));

    // update _eof_index if necessary
    if(eof){
        // first time to encounter the last substring
        if (_eof_index == numeric_limits<size_t>::max()){
            _eof_index = index + data.size();
        }
        // not first time to encounter the last substring
        else if (_eof_index != index + data.size()){
            throw runtime_error( "There is an error about EOF index");
        }
    }

    // store the substing into the vector
    for (size_t i = start, j = start - index; i < end; ++i, ++j){
        if ( _stream[i % _capacity].second == true){
            if (_stream[i %_capacity].first != data[j]){
                throw runtime_error("There is an error about inconsistent substrings");
            }
        }
        else{
            _stream[i %_capacity]= { data[j], true};
            ++_unassemble_byte_cnt;
        }
    }

    // write the substring into output
    string str = "";
    while ( _expected_index < _eof_index && _stream[_expected_index % _capacity].second == true){
        str.push_back(_stream[_expected_index % _capacity].first);
        _stream[_expected_index % _capacity] = {'\0', false};
        --_unassemble_byte_cnt;
        ++ _expected_index;
    }
    _output.write(str);

    if (_expected_index == _eof_index){
        _output.end_input();
    }
    

}

size_t StreamReassembler::unassembled_bytes() const { return _unassemble_byte_cnt; }

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
