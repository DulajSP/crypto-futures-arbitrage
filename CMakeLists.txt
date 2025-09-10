cmake_minimum_required(VERSION 3.20)
project(FuturesArbitrageBot VERSION 0.1 LANGUAGES CXX)

# Toolchain: Enable vcpkg (required for manifest mode)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# Compiler Settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output Directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include Directories
include_directories(
  include
  third_party
)

# Source Files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# Executable target
add_executable(arbitrage_bot ${SOURCES})

# Dependencies from vcpkg
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(ixwebsocket CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)


# Subdirectories for Custom Code
add_subdirectory(third_party/bip3x)
add_subdirectory(proto)  # handles all .proto compilation and linking

# secp256k1 (via FetchContent)
include(FetchContent)

FetchContent_Declare(
  secp256k1
  GIT_REPOSITORY https://github.com/bitcoin-core/secp256k1.git
  GIT_TAG master
)

set(SECP256K1_ENABLE_MODULE_ECDSA_RECOVERY ON CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_EXTRAKEYS ON CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_BENCH OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(secp256k1)


# Target Include Paths
target_include_directories(arbitrage_bot
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/proto
    ${CMAKE_CURRENT_BINARY_DIR}/proto
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link external libraries
target_link_libraries(arbitrage_bot
  PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    ixwebsocket::ixwebsocket
    protobuf::libprotobuf
    cpr::cpr
    secp256k1
    bip3x
    dydx_v4_proto
)