if (WHISPER_GST)
    # stream
    set(TARGET stream)
    add_executable(${TARGET} stream.cpp)

    include(DefaultTargetOptions)

    target_link_libraries(${TARGET} PRIVATE common common-gstreamer whisper ${CMAKE_THREAD_LIBS_INIT})
endif ()
