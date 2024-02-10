#pragma once

#include <SDL.h>
#include <SDL_audio.h>

#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>

#include <common-audioasync.h>

//
// SDL Audio capture
//
class audio_async_sdl : public audio_async {
public:
    audio_async_sdl(int len_ms);
    ~audio_async_sdl();

    bool init(whisper_params params, int sample_rate) override;

    // start capturing audio via the provided SDL callback
    // keep last len_ms seconds of audio in a circular buffer
    bool resume();
    bool pause();
    bool clear();

    // get audio data from the circular buffer
    void get(int ms, std::vector<float> & audio) override;

private:
    SDL_AudioDeviceID m_dev_id_in = 0;
};

// Return false if need to quit
bool sdl_poll_events();
