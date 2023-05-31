#include "common-gstreamer.h"


audio_gstreamer::audio_gstreamer(int len_ms) {
    m_len_ms = len_ms;   
    m_running = false;
}

audio_gstreamer::~audio_gstreamer() {
  /* Free resources */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  gst_element_set_state (app_sink, GST_STATE_NULL);
  gst_object_unref (app_sink);  
}

/* The appsink has received a sample buffer */
static GstFlowReturn gstreamer_new_sample (GstElement *sink, audio_gstreamer *streamer) {
  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo info; 
  bool error = false;

  /* Retrieve the sample and buffer */
  g_signal_emit_by_name (sink, "pull-sample", &sample);
  if (sample) {
    buffer = gst_sample_get_buffer(sample);
    if (buffer) {
      if (gst_buffer_map( buffer, &info, (GstMapFlags)(GST_MAP_READ))) {
        streamer->callback(info.data, info.size);
      } else
        error = true;
    } else
      error = true;
  } else
    error = true;

  if (sample)
    gst_sample_unref(sample);
  if (buffer)
    gst_buffer_unmap(buffer, &info);

  if (error)
    return GST_FLOW_ERROR;
  else
    return GST_FLOW_OK;
}

/* Non-threaded (no GLib mainloop) synchronous message handler */
static GstBusSyncReply audio_gstreamer_bus_sync_handler(GstBus *bus, GstMessage *msg, audio_gstreamer* gstreamer) {
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:
      GError *err;
      gchar *debug_info;
      gst_message_parse_error (msg, &err, &debug_info);
      fprintf (stderr, "\n%s: Gstreamer received an error from element %s: %s\n", __func__, GST_OBJECT_NAME (msg->src), err->message);
      fprintf (stderr, "%s: Debugging information: %s\n", __func__, debug_info ? debug_info : "none");
      g_clear_error (&err);
      g_free (debug_info);      
      gstreamer->shutdown();
      break;
    case GST_MESSAGE_EOS:
      fprintf (stderr, "%s: Gstreamer reached end of stream\n", __func__);
      gstreamer->shutdown();
      break;
  }    
  return GST_BUS_DROP;
}

bool audio_gstreamer::init(int* argc, char*** argv, std::string pipelineDescription, int sample_rate) {
    /* Initialize GStreamer */
    gst_init (argc, argv); 

    /* Create pipeline from pipeline descriptor */  
    pipeline = gst_parse_launch(pipelineDescription.c_str(), NULL);
    if (pipeline) {
        fprintf(stdout, "%s: Got gstreamer pipeline: %s\n", __func__, pipelineDescription.c_str());
    } else {
        fprintf(stderr, "%s: couldn't get Gstreamer pipeline for the following descriptor: %s!\n", __func__, pipelineDescription.c_str());
        return false;        
    }

    /* Create app sink to receive audio from main pipeline */
    app_sink = gst_element_factory_make ("appsink", "app_sink");
    if (!app_sink) {
        fprintf(stderr, "%s: couldn't create Gstreamer app sink!\n", __func__);
        return false;        
    }
    /* Configure app sink audio */
    GstAudioInfo info;
    gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_F32LE /*TODO: select BE if on big endian system*/, SAMPLE_RATE, 1, NULL);
    GstCaps *audio_caps = gst_audio_info_to_caps (&info);
    g_object_set (app_sink, "emit-signals", TRUE, "caps", audio_caps, NULL);
    // App sink will listen for new sample availability
    g_signal_connect (app_sink, "new-sample", G_CALLBACK (gstreamer_new_sample), this);
    gst_caps_unref (audio_caps);    
    // Get end of the pipeline (last element)...
    GstIterator *iter = gst_bin_iterate_sorted(GST_BIN(pipeline));
    GstElement *last_pipe_element;
    GValue value = G_VALUE_INIT;
    if (gst_iterator_next(iter, &value) == GST_ITERATOR_OK) {
      last_pipe_element = GST_ELEMENT(g_value_get_object(&value));
    } else {
      fprintf(stderr, "%s: Can't get last element in pipeline for attaching app sink", __func__);
      return 1;
    }
    g_value_unset(&value);
    gst_iterator_free(iter);    
    // ...and attach app-sink to that end
    gst_bin_add(GST_BIN(pipeline), app_sink);
    if (gst_element_link (last_pipe_element, app_sink) != TRUE) {
        fprintf(stderr, "%s: App sink could not be linked to the end of the pipeline!\n", __func__);
        gst_object_unref (pipeline);
        return false;
    }

    /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
    GstBus *bus = gst_element_get_bus (pipeline);
    gst_bus_add_signal_watch (bus);
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)audio_gstreamer_bus_sync_handler, this, NULL);   
    gst_object_unref (bus);    

    // set sample rate and initialize audio sample vector
    m_sample_rate = SAMPLE_RATE;
    m_audio.resize((m_sample_rate*m_len_ms)/1000);

    return true;
}

bool audio_gstreamer::resume() {
    if (m_running) {
        fprintf(stderr, "%s: already running!\n", __func__);
        return false;
    }

    /* Start playing */
    gst_element_set_state (pipeline, GST_STATE_PLAYING);    

    m_running = true; 

    return true;
}

bool audio_gstreamer::pause() {
    if (!m_running) {
        fprintf(stderr, "%s: already paused!\n", __func__);
        return false;
    }

    gst_element_set_state (pipeline, GST_STATE_PAUSED);   
    m_running = false;
    
    return true;
}

bool audio_gstreamer::shutdown() {
    m_running = false;
    return true;
}

bool audio_gstreamer::clear() {
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


void audio_gstreamer::get(int ms, std::vector<float> & result) {
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

// callback to be called by gstreamer callback
void audio_gstreamer::callback(uint8_t * stream, int len) {
    if (!m_running) {
        return;
    }

    const size_t n_samples = len / sizeof(float);
    m_audio_new.resize(n_samples);
    memcpy(m_audio_new.data(), stream, n_samples * sizeof(float));
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_audio_pos + n_samples > m_audio.size()) {
            const size_t n0 = m_audio.size() - m_audio_pos;
            memcpy(&m_audio[m_audio_pos], stream, n0 * sizeof(float));
            memcpy(&m_audio[0], &stream[n0], (n_samples - n0) * sizeof(float));
            m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
            m_audio_len = m_audio.size();
        } else {
            memcpy(&m_audio[m_audio_pos], stream, n_samples * sizeof(float));
            m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
            m_audio_len = std::min(m_audio_len + n_samples, m_audio.size());
        }
    }
}

bool audio_gstreamer::is_stream_running() {
    return m_running;
}