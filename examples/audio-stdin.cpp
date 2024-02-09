#include "audio-stdin.h"

#include <csignal>
#include <cstring>

// Because the original happened to handle OS signals in the same library as
// handled the audio, this is implemented here.
// TODO: split this out to something a bit more coherent

bool should_quit = false;
float S16_TO_F32_SCALE_FACTOR = 0.000030517578125f;

void quit_signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    should_quit = true;
  }
}

void install_signal_handler() {
    std::signal(SIGINT, quit_signal_handler);
    std::signal(SIGTERM, quit_signal_handler);
}

bool should_keep_running() {
  return !should_quit;
}

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
  m_audio.resize(0);  // resize to reclaim this memory, as it isn't needed (floats buffer filled on the fly)
  m_in_buffer.resize((m_sample_rate*m_len_ms)/1000);

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

        // stdin is PCM mono 16khz in s16le format.  Use ffmpeg to make that happen.
        int nread = read(STDIN_FILENO, m_in_buffer.data(), n_samples*sizeof(int16_t) /*m_in_buffer.size()*/);
        if (nread <= 0) { 
            result.resize(0);
            return; 
        }

        int float_sample_count = nread / sizeof(float);
        result.resize(float_sample_count);
        for (int i = 0; i < float_sample_count; i++) {
            result[i] = m_in_buffer[i] * S16_TO_F32_SCALE_FACTOR;
        }
    }
}