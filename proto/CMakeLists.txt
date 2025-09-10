# proto/CMakeLists.txt

cmake_minimum_required(VERSION 3.15)

find_package(Protobuf REQUIRED)

# Disable appending full proto paths to #includes
set(PROTOBUF_GENERATE_CPP_APPEND_PATH OFF)

# Recursively find all .proto files in this directory
file(GLOB_RECURSE proto_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.proto)

# Generate C++ source and header files from .proto
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${proto_files})

# Create a static or shared library from the generated files
add_library(dydx_v4_proto ${PROTO_SRCS} ${PROTO_HDRS})

# Add the include directories for generated code and Protobuf headers
target_include_directories(dydx_v4_proto PUBLIC
    ${Protobuf_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Link against Protobuf
target_link_libraries(dydx_v4_proto PUBLIC ${Protobuf_LIBRARIES})