#include "common-grpc.h"


#include <fstream>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <stdio.h>

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include "transcription.grpc.pb.h"


using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using sigper::transcription::AudioTranscription;
using sigper::transcription::AudioSegmentRequest;
using sigper::transcription::TranscriptResponse;


audio_async::audio_async(int len_ms) {
    m_len_ms = len_ms;
    m_running = false;
}

audio_async::~audio_async() {
    Shutdown();
}

bool audio_async::init(int server_port, int sample_rate) {

    m_sample_rate = sample_rate;

     m_audio.resize((m_sample_rate*m_len_ms)/1000);

    m_server_address = "0.0.0.0:"+ std::to_string(server_port);

    grpc::EnableDefaultHealthCheckService(true);

    StartAsyncService(m_server_address);
    fprintf(stdout, "\n***************** Started server at: %s *****************\n", m_server_address.c_str());

    return true;
}

void audio_async::StartAsyncService(std::string server_address) {
    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());    
    builder.RegisterService(&m_service);
    mup_cq = builder.AddCompletionQueue();
    mup_server = builder.BuildAndStart();

    StartNewRpcConnectionListner();

    mup_grpc_thread.reset(new std::thread(
        (std::bind(&audio_async::GrpcThread, this))));   
}

void audio_async::Shutdown() {
    mup_server->Shutdown();
    mup_cq->Shutdown();  
}

//
// BEGIN GPRC PROCESSING
//
  void audio_async::StartNewRpcConnectionListner() {
    std::unique_ptr<ServerContext> context = std::make_unique<ServerContext>();
    // This initiates a single stream for a single client. To allow multiple
    // clients in different threads to connect, simply 'request' from the
    // different threads. Each stream is independent but can use the same
    // completion queue/context objects.
    mup_stream.reset(
        new ServerAsyncReaderWriter<TranscriptResponse, AudioSegmentRequest>(context.get()));
    m_service.RequestTranscribeAudio(context.get(), mup_stream.get(), mup_cq.get(), mup_cq.get(),
        reinterpret_cast<void*>(TagType::CONNECT));
    // This is important as the server should know when the client is done.
    context->AsyncNotifyWhenDone(reinterpret_cast<void*>(TagType::DONE));    
    m_running = true;
  }


void audio_async::GrpcThread() {
    while (true) {
        void* got_tag = nullptr;
        bool ok = false;
        if (!mup_cq->Next(&got_tag, &ok)) {
            break;
        }

        if (ok) {
            // Inline event handler and quasi-state machine (though this handles asynch reads/writes, so a strict SM is not applicable)
            switch (static_cast<TagType>(reinterpret_cast<size_t>(got_tag))) {
            case TagType::READ:
                // Read done, great -- just wait for next activity (read or new external write) now that it is out
                IngestAudioData();
                WaitForRequest();
                break;
            case TagType::CONNECT:
                m_connected = true;
                WaitForRequest();
                break;
            case TagType::WRITE:
                // Async write done, great -- just wait for next activity (read or new external write) now that it is out
                break;
            case TagType::DONE:
                m_connected = false;
                break;
            case TagType::FINISH:
                m_running = false;
                break;
            default:
                assert(false);
            }
        } else {
            m_running = false;
            StartNewRpcConnectionListner();
        }
    }
}

void audio_async::WaitForRequest() {
    if (m_connected) {
        // In the case of the server, we wait for a READ first and then write a
        // response. A server cannot initiate a connection so the server has to
        // wait for the client to send a message in order for it to respond back.
        mup_stream->Read(&m_request, reinterpret_cast<void*>(TagType::READ));
    }
}


static inline std::vector<float> convert_s16le_string_data_to_floats(std::string strdata) {
    std::vector<int16_t> intdata(strdata.size() / sizeof(int16_t));
    std::vector<float> floats(intdata.size());
    memcpy(intdata.data(), strdata.data(), strdata.size());     // assumes little endian system
    int i = 0;
    for (auto& d : intdata) {
        floats.at(i++) = (float) d / 32768.0f;
    }
    return floats;
} 

void audio_async::IngestAudioData() {
    std::vector<float> sampleData = convert_s16le_string_data_to_floats(m_request.audio_data());
    this->callback((uint8_t*) sampleData.data(), sampleData.size()*sizeof(float));
}

void audio_async::SendTranscription(std::string transcript, int seq_num,
    std::time_t start_time, std::time_t end_time) 
{
    if (m_connected) {
      TranscriptResponse response;
      response.set_transcription(transcript);
      response.set_seq_num(seq_num);
      //response.set_start_time(start_time);
      //response.set_end_time(end_time);
      mup_stream->Write(response, reinterpret_cast<void*>(TagType::WRITE));
    } else {
      fprintf(stderr, ">>>>>> CANNOT SEND TRANSCRIPTION -- NOT RUNNING");
    }
}
//
// END GRPC PROCESSING
//

bool audio_async::resume() {

    if (m_running) {
        fprintf(stderr, "%s: already running!\n", __func__);
        return false;
    }

    m_running = true;

    return true;
}

bool audio_async::pause() {

    if (!m_running) {
        fprintf(stderr, "%s: already paused!\n", __func__);
        return false;
    }

    m_running = false;

    return true;
}

bool audio_async::clear() {

    if (!m_running) {
        fprintf(stderr, "%s: not running!\n", __func__);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_audio_pos = 0;
        m_audio_len = 0;
    }

    return true;
}

// callback to be called by SDL
void audio_async::callback(uint8_t * stream, int len) {

    if (!m_running) {
        return;
    }

    size_t n_samples = len / sizeof(float);

    if (n_samples > m_audio.size()) {
        n_samples = m_audio.size();

        stream += (len - (n_samples * sizeof(float)));
    }

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

void audio_async::get(int ms, std::vector<float> & result) {

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

bool audio_async::is_running() {
    return m_running;
}