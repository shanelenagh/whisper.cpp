#pragma once


#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>
#include <thread>
#include <cstring>


namespace audioasync_constants {
  const float S16_TO_F32_SCALE_FACTOR = 0.000030517578125f;
}

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
    std::string input_src = "stdin";     // TODO: Make conditional based on whether SDL available at compile time
};


void whisper_print_usage(int argc, char ** argv, const whisper_params & params);

inline bool whisper_params_parse(int argc, char ** argv, whisper_params & params) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
        else if (arg == "-i"    || arg == "--input")         { params.input_src  = argv[++i]; }
        else if (arg == "-t"    || arg == "--threads")       { params.n_threads     = std::stoi(argv[++i]); }
        else if (                  arg == "--step")          { params.step_ms       = std::stoi(argv[++i]); }
        else if (                  arg == "--length")        { params.length_ms     = std::stoi(argv[++i]); }
        else if (                  arg == "--keep")          { params.keep_ms       = std::stoi(argv[++i]); }
        else if (arg == "-c"    || arg == "--capture")       { params.capture_id    = std::stoi(argv[++i]); }
        else if (arg == "-mt"   || arg == "--max-tokens")    { params.max_tokens    = std::stoi(argv[++i]); }
        else if (arg == "-ac"   || arg == "--audio-ctx")     { params.audio_ctx     = std::stoi(argv[++i]); }
        else if (arg == "-vth"  || arg == "--vad-thold")     { params.vad_thold     = std::stof(argv[++i]); }
        else if (arg == "-fth"  || arg == "--freq-thold")    { params.freq_thold    = std::stof(argv[++i]); }
        else if (arg == "-su"   || arg == "--speed-up")      { params.speed_up      = true; }
        else if (arg == "-tr"   || arg == "--translate")     { params.translate     = true; }
        else if (arg == "-nf"   || arg == "--no-fallback")   { params.no_fallback   = true; }
        else if (arg == "-ps"   || arg == "--print-special") { params.print_special = true; }
        else if (arg == "-kc"   || arg == "--keep-context")  { params.no_context    = false; }
        else if (arg == "-l"    || arg == "--language")      { params.language      = argv[++i]; }
        else if (arg == "-m"    || arg == "--model")         { params.model         = argv[++i]; }
        else if (arg == "-f"    || arg == "--file")          { params.fname_out     = argv[++i]; }
        else if (arg == "-tdrz" || arg == "--tinydiarize")   { params.tinydiarize   = true; }
        else if (arg == "-sa"   || arg == "--save-audio")    { params.save_audio    = true; }
        else if (arg == "-ng"   || arg == "--no-gpu")        { params.use_gpu       = false; }

        else {
            fprintf(stderr, "error: unknown argument: %s\n", arg.c_str());
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
    }

    return true;
}

inline void whisper_print_usage(int /*argc*/, char ** argv, const whisper_params & params) {
    fprintf(stderr, "\n");
    fprintf(stderr, "usage: %s [options]\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -h,       --help          [default] show this help message and exit\n");
    fprintf(stderr, "  -i SRC,     --input SRC   [%-7s] Input source (SDL, stdin)\n",                      params.input_src.c_str());    
    fprintf(stderr, "  -t N,     --threads N     [%-7d] number of threads to use during computation\n",    params.n_threads);
    fprintf(stderr, "            --step N        [%-7d] audio step size in milliseconds\n",                params.step_ms);
    fprintf(stderr, "            --length N      [%-7d] audio length in milliseconds\n",                   params.length_ms);
    fprintf(stderr, "            --keep N        [%-7d] audio to keep from previous step in ms\n",         params.keep_ms);
    fprintf(stderr, "  -c ID,    --capture ID    [%-7d] capture device ID\n",                              params.capture_id);
    fprintf(stderr, "  -mt N,    --max-tokens N  [%-7d] maximum number of tokens per audio chunk\n",       params.max_tokens);
    fprintf(stderr, "  -ac N,    --audio-ctx N   [%-7d] audio context size (0 - all)\n",                   params.audio_ctx);
    fprintf(stderr, "  -vth N,   --vad-thold N   [%-7.2f] voice activity detection threshold\n",           params.vad_thold);
    fprintf(stderr, "  -fth N,   --freq-thold N  [%-7.2f] high-pass frequency cutoff\n",                   params.freq_thold);
    fprintf(stderr, "  -su,      --speed-up      [%-7s] speed up audio by x2 (reduced accuracy)\n",        params.speed_up ? "true" : "false");
    fprintf(stderr, "  -tr,      --translate     [%-7s] translate from source language to english\n",      params.translate ? "true" : "false");
    fprintf(stderr, "  -nf,      --no-fallback   [%-7s] do not use temperature fallback while decoding\n", params.no_fallback ? "true" : "false");
    fprintf(stderr, "  -ps,      --print-special [%-7s] print special tokens\n",                           params.print_special ? "true" : "false");
    fprintf(stderr, "  -kc,      --keep-context  [%-7s] keep context between audio chunks\n",              params.no_context ? "false" : "true");
    fprintf(stderr, "  -l LANG,  --language LANG [%-7s] spoken language\n",                                params.language.c_str());
    fprintf(stderr, "  -m FNAME, --model FNAME   [%-7s] model path\n",                                     params.model.c_str());
    fprintf(stderr, "  -f FNAME, --file FNAME    [%-7s] text output file name\n",                          params.fname_out.c_str());
    fprintf(stderr, "  -tdrz,    --tinydiarize   [%-7s] enable tinydiarize (requires a tdrz model)\n",     params.tinydiarize ? "true" : "false");
    fprintf(stderr, "  -sa,      --save-audio    [%-7s] save the recorded audio to a file\n",              params.save_audio ? "true" : "false");
    fprintf(stderr, "  -ng,      --no-gpu        [%-7s] disable GPU inference\n",                          params.use_gpu ? "false" : "true");
    fprintf(stderr, "\n");
}    

//  500 -> 00:05.000
// 6000 -> 01:00.000
inline std::string to_timestamp(int64_t t) {
    int64_t sec = t/100;
    int64_t msec = t - sec*100;
    int64_t min = sec/60;
    sec = sec - min*60;

    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d.%03d", (int) min, (int) sec, (int) msec);

    return std::string(buf);
}


//
// Abstract class for audio capture
//
template <typename T = float> class audio_async {
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

    bool resume() {
        m_running = true;
        return true;
    }

    bool pause() {
        m_running = false;
        return true;
    }

    bool clear() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_audio_pos = 0;
            m_audio_len = 0;
        }
        return true;
    }

    bool is_running() {
        return m_running;
    }

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

            transfer_buffer(result, s0, n_samples);
        }        
    }

    // buffer callback persistence to be called by audio source ingester
    void callback(uint8_t * stream, int len) {
        if (!m_running) {
            return;
        }

        size_t n_samples = len / sizeof(float);

        if (n_samples > m_audio.size()) {
            n_samples = m_audio.size();

            stream += (len - (n_samples * sizeof(T)));
        }

        //fprintf(stderr, "%s: %zu samples, pos %zu, len %zu\n", __func__, n_samples, m_audio_pos, m_audio_len);

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_audio_pos + n_samples > m_audio.size()) {
                const size_t n0 = m_audio.size() - m_audio_pos;

                memcpy(&m_audio[m_audio_pos], stream, n0 * sizeof(T));
                memcpy(&m_audio[0], stream + n0 * sizeof(T), (n_samples - n0) * sizeof(T));

                m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
                m_audio_len = m_audio.size();
            } else {
                memcpy(&m_audio[m_audio_pos], stream, n_samples * sizeof(T));

                m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
                m_audio_len = std::min(m_audio_len + n_samples, m_audio.size());
            }
        }
    }

    static void * create(whisper_params params);

protected:
    int m_len_ms = 0;
    int m_sample_rate = 0;

    std::atomic_bool m_running;
    std::mutex       m_mutex;

    std::vector<T> m_audio;
    size_t             m_audio_pos = 0;
    size_t             m_audio_len = 0;

    void transfer_buffer(std::vector<float> & result, int start, int sampleCount);
};

#include "common-sdl.h"
//#include "audio-stdin.h"

inline void * createAsyncAudio(whisper_params params) {
    if (params.input_src == "sdl") {
        //return new audio_async_sdl(params.length_ms);
    } else if (params.input_src == "stdin") {
        return NULL; //return new audio_async_stdin(params.length_ms);
    } else {
        fprintf(stderr, "%s: Unknown input src: %s\n", __func__, params.input_src.c_str());
        //whisper_print_usage();
        return NULL;
    }
}


// Fast memcpy transfer for float
template <> inline void audio_async<float>::transfer_buffer(std::vector<float> & result, int start, int sampleCount) {
    result.resize(sampleCount);
    if (start + sampleCount > m_audio.size()) {
        const size_t n0 = m_audio.size() - start;

        memcpy(result.data(), &m_audio[start], n0 * sizeof(float));
        memcpy(&result[n0], &m_audio[0], (sampleCount - n0) * sizeof(float));
    } else {
        memcpy(result.data(), &m_audio[start], sampleCount * sizeof(float));
    }        
}  

// Conversion from int16_t to float for s16le buffers
template <> inline void audio_async<int16_t>::transfer_buffer(std::vector<float> & result, int start, int sampleCount) {
    result.resize(sampleCount);
    if (start + sampleCount > m_audio.size()) {
        const size_t n0 = m_audio.size() - start;

        for (int i = 0; i < n0; i++) {
            result[i] = m_audio[start + i] * audioasync_constants::S16_TO_F32_SCALE_FACTOR;
        }
        for (int i = n0; i < sampleCount; i++) {
            result[i] = m_audio[i] * audioasync_constants::S16_TO_F32_SCALE_FACTOR;
        }
    } else {
        for (int i = 0; i < sampleCount; i++) {
            result[i] = m_audio[start + i] * audioasync_constants::S16_TO_F32_SCALE_FACTOR;
        }
    }        
}