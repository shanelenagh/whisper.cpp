#include "common-grpc.h"


#include <fstream>
#include <string>
#include <iostream>
#include <thread>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
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


enum class Type { READ = 1, WRITE = 2, CONNECT = 3, DONE = 4, FINISH = 5 };

/*
AudioTranscriptionServiceImpl::AudioTranscriptionServiceImpl(audio_async *callback_audio) {
    m_audio_async = callback_audio;
}
*/

// synchronous Transcription service implementation
/*
Status audio_async::TranscribeAudio(ServerContext* context,
                ServerReaderWriter<TranscriptResponse, AudioSegmentRequest>* stream) 
{
    std::cout << "Got request\n";
    AudioSegmentRequest audio;
    while (stream->Read(&audio)) {
        std::cout << "Read one with data: " << audio.audio_data() << std::endl;
        ++seq_num;
        std::cout << "Now writing to file " << seq_num << ".bin" << std::endl;
        std::ofstream fout;
        fout.open(std::to_string(seq_num)+".bin", std::ios::binary | std::ios::out);
        fout.write(audio.audio_data().data(), audio.audio_data().length());
        fout.close();

        TranscriptResponse transcript;
        transcript.set_seq_num(seq_num);
        transcript.set_transcription("Got message\n");
        stream->Write(transcript);
        std::cout << "Wrote response" << std::endl;
        //stream->WritesDone();
        this->callback(audio.audio_data());
    }    

    std::cout << "Returning" << std::endl;

    return Status::OK;
}
*/


audio_async::audio_async(int len_ms) {
    m_len_ms = len_ms;

    m_running = false;
}

audio_async::~audio_async() {
    // if (m_dev_id_in) {
    //     SDL_CloseAudioDevice(m_dev_id_in);
    // }
    Shutdown();
}

bool audio_async::init(int server_port, int sample_rate) {

    m_server_address = "0.0.0.0:"+ std::to_string(server_port);

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    //m_service = std::make_unique<AudioTranscriptionServiceImpl>(this);

    // SYNCH CALL
    // Finally assemble the server.
    //std::unique_ptr<Server> server(builder.BuildAndStart());
    //std::cout << "Server listening on " << server_address << std::endl;

    // ASYNCH SEMATICS
    // std::cout << "Server listening on " << server_address << std::endl;
    // HandleRpcs();

    // ASYNC example - WORKS! :-)
    //HandleRpcs();

    // ASYNC Gist
        // This initiates a single stream for a single client. To allow multiple
        // clients in different threads to connect, simply 'request' from the
        // different threads. Each stream is independent but can use the same
        // completion queue/context objects.
    StartAsyncService(m_server_address);


    return true;
}

void audio_async::StartAsyncService(std::string server_address) {
    mup_service = std::make_unique<AudioTranscription::AsyncService>();
    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());    
    builder.RegisterService(mup_service.get());
    mup_cq = builder.AddCompletionQueue();
    mup_server = builder.BuildAndStart();
    mup_stream.reset(
        new ServerAsyncReaderWriter<TranscriptResponse, AudioSegmentRequest>(&m_context));
    mup_service->RequestTranscribeAudio(&m_context, mup_stream.get(), mup_cq.get(), mup_cq.get(),
        reinterpret_cast<void*>(Type::CONNECT));    
    m_context.AsyncNotifyWhenDone(reinterpret_cast<void*>(Type::DONE));
    mup_grpc_thread.reset(new std::thread(
        (std::bind(&audio_async::GrpcThread, this))));   
    std::cout << "Server listening on " << server_address << std::endl;    


    m_running = true;
}

void audio_async::Shutdown() {
        //mup_grpc_thread->join();
    mup_server->Shutdown();
    mup_cq->Shutdown();  
    //delete m_service;
}





//
// BEGIN GIST
//
void audio_async::GrpcThread() {
    std::cout << "GRPC thread running" << std::endl;    

    while (m_running) {
        void* got_tag = nullptr;
        bool ok = false;
        if (!mup_cq->Next(&got_tag, &ok)) {
            std::cerr << "Server stream closed. Quitting" << std::endl;
            break;
        }

        //assert(ok);

        if (ok) {
            std::cout << std::endl
                << "**** Processing completion queue tag " << got_tag
                << std::endl;
            switch (static_cast<Type>(reinterpret_cast<size_t>(got_tag))) {
            case Type::READ:
                std::cout << "Read a new message." << std::endl;
                AsyncSendResponse();
                break;
            case Type::WRITE:
                std::cout << "Sending message (async)." << std::endl;
                AsyncWaitForRequest();
                break;
            case Type::CONNECT:
                std::cout << "Client connected." << std::endl;
                AsyncWaitForRequest();
                break;
            case Type::DONE:
                std::cout << "Server disconnecting." << std::endl;
                m_running = false;
                mup_stream->Finish(::grpc::Status::CANCELLED, reinterpret_cast<void*>(Type::DONE));
                //Shutdown();
                //StartAsyncService(m_server_address);
                break;
            case Type::FINISH:
                std::cout << "Server quitting." << std::endl;
                break;
            default:
                std::cerr << "Unexpected tag " << got_tag << std::endl;
                assert(false);
            }
        }
    }
    std::cout << "Stopping GRPC handler thread" << std::endl;
}

void audio_async::AsyncWaitForRequest() {
    if (is_running()) {
        // In the case of the server, we wait for a READ first and then write a
        // response. A server cannot initiate a connection so the server has to
        // wait for the client to send a message in order for it to respond back.
        mup_stream->Read(&m_request, reinterpret_cast<void*>(Type::READ));
        std::cout << "Read request with data: " << m_request.audio_data();
    }
}

void audio_async::AsyncSendResponse() {
    std::cout << " ** Handling request: " << m_request.audio_data() << std::endl;
    TranscriptResponse response;
    std::string resp = "I hear you: " + m_request.audio_data();
    std::cout << " ** Sending response: " << resp << std::endl;
    response.set_transcription(resp);
    mup_stream->Write(response, reinterpret_cast<void*>(Type::WRITE));
}
//
// END GIST
//

bool audio_async::resume() {
    // if (!m_dev_id_in) {
    //     fprintf(stderr, "%s: no audio device to resume!\n", __func__);
    //     return false;
    // }

    // if (m_running) {
    //     fprintf(stderr, "%s: already running!\n", __func__);
    //     return false;
    // }

    // SDL_PauseAudioDevice(m_dev_id_in, 0);

    // m_running = true;

    return true;
}

bool audio_async::pause() {
    // if (!m_dev_id_in) {
    //     fprintf(stderr, "%s: no audio device to pause!\n", __func__);
    //     return false;
    // }

    // if (!m_running) {
    //     fprintf(stderr, "%s: already paused!\n", __func__);
    //     return false;
    // }

    // SDL_PauseAudioDevice(m_dev_id_in, 1);

    // m_running = false;

    return true;
}

bool audio_async::clear() {
    // if (!m_dev_id_in) {
    //     fprintf(stderr, "%s: no audio device to clear!\n", __func__);
    //     return false;
    // }

    // if (!m_running) {
    //     fprintf(stderr, "%s: not running!\n", __func__);
    //     return false;
    // }

    // {
    //     std::lock_guard<std::mutex> lock(m_mutex);

    //     m_audio_pos = 0;
    //     m_audio_len = 0;
    // }

    return true;
}

// callback to be called by SDL
void audio_async::callback(std::string data) {

    std::cout << "Called back with: " << data << std::endl;
    // if (!m_running) {
    //     return;
    // }

    // size_t n_samples = len / sizeof(float);

    // if (n_samples > m_audio.size()) {
    //     n_samples = m_audio.size();

    //     stream += (len - (n_samples * sizeof(float)));
    // }

    // //fprintf(stderr, "%s: %zu samples, pos %zu, len %zu\n", __func__, n_samples, m_audio_pos, m_audio_len);

    // {
    //     std::lock_guard<std::mutex> lock(m_mutex);

    //     if (m_audio_pos + n_samples > m_audio.size()) {
    //         const size_t n0 = m_audio.size() - m_audio_pos;

    //         memcpy(&m_audio[m_audio_pos], stream, n0 * sizeof(float));
    //         memcpy(&m_audio[0], stream + n0 * sizeof(float), (n_samples - n0) * sizeof(float));

    //         m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
    //         m_audio_len = m_audio.size();
    //     } else {
    //         memcpy(&m_audio[m_audio_pos], stream, n_samples * sizeof(float));

    //         m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
    //         m_audio_len = std::min(m_audio_len + n_samples, m_audio.size());
    //     }
    // }
}

void audio_async::get(int ms, std::vector<float> & result) {

    std::this_thread::sleep_for (std::chrono::seconds(3));

    // if (!m_dev_id_in) {
    //     fprintf(stderr, "%s: no audio device to get audio from!\n", __func__);
    //     return;
    // }

    // if (!m_running) {
    //     fprintf(stderr, "%s: not running!\n", __func__);
    //     return;
    // }

    // result.clear();

    // {
    //     std::lock_guard<std::mutex> lock(m_mutex);

    //     if (ms <= 0) {
    //         ms = m_len_ms;
    //     }

    //     size_t n_samples = (m_sample_rate * ms) / 1000;
    //     if (n_samples > m_audio_len) {
    //         n_samples = m_audio_len;
    //     }

    //     result.resize(n_samples);

    //     int s0 = m_audio_pos - n_samples;
    //     if (s0 < 0) {
    //         s0 += m_audio.size();
    //     }

    //     if (s0 + n_samples > m_audio.size()) {
    //         const size_t n0 = m_audio.size() - s0;

    //         memcpy(result.data(), &m_audio[s0], n0 * sizeof(float));
    //         memcpy(&result[n0], &m_audio[0], (n_samples - n0) * sizeof(float));
    //     } else {
    //         memcpy(result.data(), &m_audio[s0], n_samples * sizeof(float));
    //     }
    // }
}

bool audio_async::is_running() {
    return m_running;
}



