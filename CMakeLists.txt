# ---------------------------------------------------------------------------
# DBNetwork
# ---------------------------------------------------------------------------

project(dbnetwork)
cmake_minimum_required(VERSION 3.7)

# ---------------------------------------------------------------------------
# Environment
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -rdynamic")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set DEBUG flag on debug configuration builds
add_compile_options("$<$<CONFIG:DEBUG>:-DDEBUG>")

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

find_package(Threads REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG ON)

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------

include_directories(
        ${CMAKE_SOURCE_DIR}/config
)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

add_subdirectory("client")
add_subdirectory("server")
