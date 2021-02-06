file(GLOB_RECURSE CLIENT **.cpp **/**.cpp **.hpp **/**.hpp)
add_executable(client client.cpp ${CLIENT})
target_link_libraries(client shared)
