#pragma once

#include <string>

struct NetQuery
{
    int socket;
    std::string data;
    NetQuery(int socket) : socket(socket) {};
};

