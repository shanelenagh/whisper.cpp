#pragma once


#include <atomic>
#include <cstdint>
#include <ctime>
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
class audio_async {
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

    // get audio data from the circular buffer
    void get(int ms, std::vector<float> & audio);
    void SendTranscription(std::string transcript, int seq_num,
        std::time_t start_time = std::time(0), std::time_t end_time= std::time(0)) ;

private:
    enum class TagType { READ = 1, WRITE = 2, CONNECT = 3, DONE = 4, FINISH = 5 };
    // GRPC service methods
    void StartNewRpcConnectionListner();
    void GrpcThread();
    void WaitForRequest();
    void IngestAudioData();
    void StartAsyncService(std::string server_address);

    void Shutdown();

    // callback to be called by gRPC
    void callback(uint8_t * stream, int len);    

    int seq_num = 0;
    int m_len_ms = 0;
    int m_sample_rate = 0;
    //std::unique_ptr<AudioTranscriptionServiceImpl> m_service;

    std::atomic_bool m_running = false;
    std::atomic_bool m_connected = false;
    std::mutex       m_mutex;

    std::vector<float> m_audio;
    size_t             m_audio_pos = 0;
    size_t             m_audio_len = 0;


    // Async GRPC service support
    std::unique_ptr<ServerCompletionQueue> mup_cq;
    std::unique_ptr<Server> mup_server;  
    std::unique_ptr<ServerAsyncReaderWriter<TranscriptResponse, AudioSegmentRequest>> mup_stream;
    AudioTranscription::AsyncService m_service;
    AudioSegmentRequest m_request;
    std::unique_ptr<std::thread> mup_grpc_thread;
    std::string m_server_address;
};