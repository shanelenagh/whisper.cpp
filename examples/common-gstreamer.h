#pragma once

#include <gst/gst.h>
#include <gst/audio/audio.h>

#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>

#define CHUNK_SIZE  1024  /* Amount of bytes we are sending in each buffer */
#define SAMPLE_RATE 16000 /* Samples per second we are sending */

//
// Gstreamer Audio capture (network RTSP, local file, etc.)
//

class audio_gstreamer {
public:
    audio_gstreamer(int len_ms);
    ~audio_gstreamer();

    bool init(int* argc, char*** argv, std::string pipelineDescription, int sample_rate);

    // start capturing audio via the provided Gstreamer callback
    // keep last len_ms seconds of audio in a circular buffer
    bool resume();
    bool pause();
    bool clear();
    bool shutdown();

    // get audio data from the circular buffer
    void get(int ms, std::vector<float> & audio);

    // callback to be called by SDL
    void callback(uint8_t * stream, int len);

    // Return false if need to quit
    bool is_stream_running();

private:

    GstElement *pipeline, *app_sink;

    int m_len_ms = 0;
    int m_sample_rate = 0;

    std::atomic_bool m_running;

    std::vector<float> m_audio;
    std::vector<float> m_audio_new;
    size_t             m_audio_pos = 0;
    size_t             m_audio_len = 0;
    std::mutex         m_mutex;    

};


