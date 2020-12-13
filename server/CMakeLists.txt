file(GLOB_RECURSE SERVER **.cpp **/**.cpp **.hpp **/**.hpp)
add_executable(server main.cpp ${SERVER})
target_include_directories(server PRIVATE ${CMAKE_CURRENT_LIST_DIR})
