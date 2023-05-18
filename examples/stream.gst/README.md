# stream.gst

This is a naive example of performing real-time inference on audio streams through GSTreamer streams/sources, 
using the RTSP protocol by default (Gstreamer pipeline can be overridden with a CLI switch to support other protocols and sources).
This can be used to perform streaming translation for RTSP security cameras or IP telephony applications (e.g., call center phone call streams published to an RTSP server like rtsp-simple-server/mediamtx or Gstreamer's own RTSP server).  Definitely consult your local laws to know if which applications are legal in your area if using in security applications (e.g., 3rd party audio monitoring or recording/transcription).  The use case that motivated this development was for call center support.

```bash
./stream.gst -m ./models/ggml-base.en.bin -t 8 --step 500 --length 5000
```

[[TODO: SCREENCAST OF RTSP READING OF, SAY, SECURITY CAMERA]]

## Sliding window mode with VAD

Setting the `--step` argument to `0` enables the sliding window mode:

```bash
 ./stream.gst -u rtsp://yourCameraOrPhoneMediaIp/streamChannel -m ./models/ggml-small.en.bin -t 6 --step 0 --length 30000 -vth 0.6
```

In this mode, the tool will transcribe only after some speech activity is detected. A very
basic VAD detector is used, but in theory a more sophisticated approach can be added. The
`-vth` argument determines the VAD threshold - higher values will make it detect silence more often.
It's best to tune it to the specific use case, but a value around `0.6` should be OK in general.
When silence is detected, it will transcribe the last `--length` milliseconds of audio and output
a transcription block that is suitable for parsing.

## Building

The `stream.gst` tool depends on GStreamer library to capture audio from a stream (e.g., RTSP). You can build it like this:

```bash
# Install Gstreamer on Linux
sudo apt-get install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-tools gstreamer1.0-x gstreamer1.0-alsa gstreamer1.0-gl gstreamer1.0-gtk3 gstreamer1.0-qt5 gstreamer1.0-pulseaudio

# Install Gstreamer on Mac OS
[see https://gstreamer.freedesktop.org/documentation/installing/on-mac-osx.html?gi-language=c#InstallingonMacOSX-Build]

make stream.gst
```