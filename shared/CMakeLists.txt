# ---------------------------------------------------------------------------
# Files
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SHARED_FILES **.cpp **/**.cpp **.hpp **/**.hpp)

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------
add_library(shared STATIC ${SHARED_FILES})
target_include_directories(shared PUBLIC ${CMAKE_CURRENT_LIST_DIR})
