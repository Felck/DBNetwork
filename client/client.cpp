#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <thread>

#include "Config.hpp"

constexpr auto POISSON_LAMBDA = 3.5;
constexpr auto THREAD_COUNT = 8;
constexpr auto RUN_SECONDS = 5;

void runThread(const std::atomic<bool>& keep_running)
{
  int sockfd;
  struct sockaddr_in address;
  // init socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  inet_aton(Config::serverAddress, &(address.sin_addr));
  address.sin_port = htons(Config::serverPort);
  // connect to server
  if (connect(sockfd, (struct sockaddr*)&address, sizeof(address)) == -1) {
    perror("connect()");
    exit(EXIT_FAILURE);
  }

  char buf[100];
  std::default_random_engine generator;
  std::exponential_distribution<double> distribution(POISSON_LAMBDA);

  while (keep_running) {
    int i = rand();
    std::string s = std::to_string(i);
    // send random number
    write(sockfd, s.c_str(), s.length() + 1);
    // wait for data and read in
    read(sockfd, buf, sizeof(buf));
    // check response
    if (s.compare(buf) != 0) {
      std::cerr << "Received wrong response!" << std::endl;
      exit(EXIT_FAILURE);
    }
    // wait for an exponential distributed amount of time (poisson process)
    i = 1000 * distribution(generator);
    std::this_thread::sleep_for(std::chrono::milliseconds(i));
  }

  close(sockfd);
}

int main()
{
  // start threads
  std::atomic<bool> keep_running = {true};
  std::vector<std::thread> threads;
  for (int t_i = 0; t_i < THREAD_COUNT; t_i++) {
    threads.emplace_back([&]() { runThread(keep_running); });
  }
  // wait
  sleep(RUN_SECONDS);
  // stop threads
  keep_running = false;
  for (auto& thread : threads) {
    thread.join();
  }

  std::cout << "Finished" << std::endl;

  return 0;
}
