#include "audio-stdin.h"

#include <csignal>
#include <cstring>
#include <cassert>


audio_stdin::audio_stdin(int len_ms) : audio_async(len_ms) { }

audio_stdin::~audio_stdin() {
  // Nothing to do here, we don't own m_fd
}

/*
Setup the stdin reader.  For simplicity, let's say that the file descriptor
passed in needs to already be open, and that the destructor doesn't close it.
*/
bool audio_stdin::init(whisper_params params, int sample_rate) {

  audio_async::init(params, sample_rate);
  m_audio.resize((m_sample_rate*m_len_ms)/1000);

  return true;
}

void audio_stdin::get(int ms, std::vector<float> & result) {

    if (!m_running) {
        fprintf(stderr, "%s: not running!\n", __func__);
        return;
    }

    result.clear();

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (ms <= 0) {
            ms = m_len_ms;
        }

        size_t n_samples = (m_sample_rate * ms) / 1000;

        assert(n_samples <= m_audio.size()/sizeof(int16_t));
        // stdin is PCM mono 16khz in s16le format.  Use ffmpeg to make that happen.
        int nread = read(STDIN_FILENO, m_audio.data(), n_samples*sizeof(int16_t) /*m_in_buffer.size()*/);
        if (nread <= 0) { 
          m_running = false;
          return; 
        } 
        transfer_buffer(result, 0, nread / sizeof(int16_t));
    }
}