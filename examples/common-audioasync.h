#pragma once


#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>
#include <thread>
#include <cstring>

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
    audio_async(int len_ms) { 
        m_len_ms = len_ms;

        m_running = false;
    };
    ~audio_async() { };

    virtual bool init(whisper_params params, int sample_rate) {
        m_sample_rate = sample_rate;

        m_audio.resize((m_sample_rate*m_len_ms)/1000);       

        return true; 
    }

    virtual bool resume() {
        m_running = true;
        return true;
    }
    virtual bool pause() {
        m_running = false;
        return true;
    }
    virtual bool clear() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_audio_pos = 0;
            m_audio_len = 0;
        }
        return true;
    }
    bool is_running() { return m_running; }

    // get audio data from the circular buffer
    virtual void get(int ms, std::vector<float> & result) {
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
            if (n_samples > m_audio_len) {
                n_samples = m_audio_len;
            }

            result.resize(n_samples);

            int s0 = m_audio_pos - n_samples;
            if (s0 < 0) {
                s0 += m_audio.size();
            }

            if (s0 + n_samples > m_audio.size()) {
                const size_t n0 = m_audio.size() - s0;

                memcpy(result.data(), &m_audio[s0], n0 * sizeof(float));
                memcpy(&result[n0], &m_audio[0], (n_samples - n0) * sizeof(float));
            } else {
                memcpy(result.data(), &m_audio[s0], n_samples * sizeof(float));
            }
        }        
    }

    // callback to be called by audio source
    void callback(uint8_t * stream, int len) {
        if (!m_running) {
            return;
        }

        size_t n_samples = len / sizeof(float);

        if (n_samples > m_audio.size()) {
            n_samples = m_audio.size();

            stream += (len - (n_samples * sizeof(float)));
        }

        //fprintf(stderr, "%s: %zu samples, pos %zu, len %zu\n", __func__, n_samples, m_audio_pos, m_audio_len);

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_audio_pos + n_samples > m_audio.size()) {
                const size_t n0 = m_audio.size() - m_audio_pos;

                memcpy(&m_audio[m_audio_pos], stream, n0 * sizeof(float));
                memcpy(&m_audio[0], stream + n0 * sizeof(float), (n_samples - n0) * sizeof(float));

                m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
                m_audio_len = m_audio.size();
            } else {
                memcpy(&m_audio[m_audio_pos], stream, n_samples * sizeof(float));

                m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
                m_audio_len = std::min(m_audio_len + n_samples, m_audio.size());
            }
        }
    }

private:
    int m_len_ms = 0;
    int m_sample_rate = 0;

    std::atomic_bool m_running;
    std::mutex       m_mutex;

    std::vector<float> m_audio;
    size_t             m_audio_pos = 0;
    size_t             m_audio_len = 0;
};