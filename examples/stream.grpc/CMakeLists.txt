# Copyright 2018 gRPC authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# cmake build file for C++ helloworld example.
# Assumes protobuf and gRPC have been installed using cmake.
# See cmake_externalproject/CMakeLists.txt for all-in-one cmake build
# that automatically builds all the dependencies before building helloworld.

cmake_minimum_required(VERSION 3.8)

project(stream.grpc C CXX)

include(./cmake/GRPC.cmake)

# Proto file
get_filename_component(sg_proto "./transcription.proto" ABSOLUTE)
get_filename_component(sg_proto_path "${sg_proto}" PATH)

# Generated sources
set(sg_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/transcription.pb.cc")
set(sg_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/transcription.pb.h")
set(sg_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/transcription.grpc.pb.cc")
set(sg_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/transcription.grpc.pb.h")
add_custom_command(
      OUTPUT "${sg_proto_srcs}" "${sg_proto_hdrs}" "${sg_grpc_srcs}" "${sg_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${sg_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${sg_proto}"
      DEPENDS "${sg_proto}")

# Common deps
set(sg_common_srcs "${CMAKE_CURRENT_BINARY_DIR}/common-grpc.cpp")
set(sg_common_hdrs "${CMAKE_CURRENT_BINARY_DIR}/common-grpc.h")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_library(sg_grpc_proto
  ${sg_grpc_srcs}
  ${sg_grpc_hdrs}
  ${sg_proto_srcs}
  ${sg_proto_hdrs}
)
add_library(sg_common
  ${sg_common_srcs}
  ${sg_common_hdrs}
)
target_link_libraries(sg_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})


set(TARGET stream.grpc)
add_executable(${TARGET} stream.cpp)

include(DefaultTargetOptions)

target_link_libraries(${TARGET} PRIVATE 
  common 
  whisper 
  ${CMAKE_THREAD_LIBS_INIT}
    sg_grpc_proto
    sg_common
    absl::flags
    absl::flags_parse
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}  
)