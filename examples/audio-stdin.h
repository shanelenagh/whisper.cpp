#pragma once

#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>

#include <common-audioasync.h>

//
// Stdin wav capture
//
class audio_stdin  : public audio_async {
public:
    audio_stdin(int len_ms);
    ~audio_stdin();

    bool init(whisper_params params, int sample_rate) override;

    // get audio data from the circular buffer
    // Returns false if the stream's closed.
    void get(int ms, std::vector<float> & audio) override;

private:
    // Since the data we plan on receiving needs converting, we need somewhere to hold it while we do that
    std::vector<int16_t> m_in_buffer;    
};

// Return false if need to quit - goes false at eof?
bool should_keep_running();
// Call this before needing to quit.
void install_signal_handler();