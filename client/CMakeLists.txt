file(GLOB_RECURSE CLIENT **.cpp **/**.cpp **.hpp **/**.hpp)
add_executable(client client.cpp ${CLIENT})
