#pragma once


#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>
#include <thread>

// command-line parameters
struct whisper_params {
    int32_t n_threads  = std::min(4, (int32_t) std::thread::hardware_concurrency());
    int32_t step_ms    = 3000;
    int32_t length_ms  = 10000;
    int32_t keep_ms    = 200;
    int32_t capture_id = -1;
    int32_t max_tokens = 32;
    int32_t audio_ctx  = 0;

    float vad_thold    = 0.6f;
    float freq_thold   = 100.0f;

    bool speed_up      = false;
    bool translate     = false;
    bool no_fallback   = false;
    bool print_special = false;
    bool no_context    = true;
    bool no_timestamps = false;
    bool tinydiarize   = false;
    bool save_audio    = false; // save audio to wav file
    bool use_gpu       = true;

    std::string language  = "en";
    std::string model     = "models/ggml-base.en.bin";
    std::string fname_out;
};

//
// Abstract interface for audio capture
//
class audio_async {
public:
    audio_async(int len_ms) { };
    ~audio_async() { };

    virtual bool init(whisper_params params, int sample_rate) = 0;

    virtual bool resume() = 0;
    virtual bool pause() = 0;
    virtual bool clear() = 0;

    // get audio data from the circular buffer
    virtual void get(int ms, std::vector<float> & audio) = 0;

    // callback to be called by audio source
    virtual void callback(uint8_t * stream, int len) = 0;
};