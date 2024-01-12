#pragma once


#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>
#include <string>
#include <thread>

#include "transcription.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using sigper::transcription::AudioTranscription;
using sigper::transcription::AudioSegmentRequest;
using sigper::transcription::TranscriptResponse;
using grpc::Server;
using grpc::ServerAsyncReaderWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

//
// gRPC server audio capture
//
class audio_async /*: public AudioTranscription::Service */{
public:
    audio_async(int len_ms);
    ~audio_async();

    bool init(int server_port, int sample_rate);

    // start capturing audio via the provided SDL callback
    // keep last len_ms seconds of audio in a circular buffer
    bool resume();
    bool pause();
    bool clear();
    bool is_running();

    // SYNCHRONOUS GRPC service method
    /*Status TranscribeAudio(ServerContext* context,
                   ServerReaderWriter<TranscriptResponse, AudioSegmentRequest>* stream) override;    */

    // callback to be called by SDL
    void callback(std::string data);

    // get audio data from the circular buffer
    void get(int ms, std::vector<float> & audio);

private:
    //SDL_AudioDeviceID m_dev_id_in = 0;
    void HandleRpcs();

    //gist stuff
    void GrpcThread();
    void AsyncWaitForRequest();
    void AsyncSendResponse();

    int seq_num = 0;
    int m_len_ms = 0;
    int m_sample_rate = 0;
    //std::unique_ptr<AudioTranscriptionServiceImpl> m_service;

    std::atomic_bool m_running;
    std::mutex       m_mutex;

    std::vector<float> m_audio;
    size_t             m_audio_pos = 0;
    size_t             m_audio_len = 0;


    // Async GRPC service support
    std::unique_ptr<ServerCompletionQueue> mup_cq;
    std::unique_ptr<Server> mup_server;  
    std::unique_ptr<ServerAsyncReaderWriter<TranscriptResponse, AudioSegmentRequest>> mup_stream;
    ServerContext m_context;
    // Transcription service classes
    AudioTranscription::AsyncService m_service;
    AudioSegmentRequest m_request;

    //gist stuff
    std::unique_ptr<std::thread> grpc_thread_;
};