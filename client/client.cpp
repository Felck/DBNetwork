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

#include "workload.hpp"

constexpr auto USE_POISSON = false;
constexpr auto POISSON_LAMBDA = 3.5;
constexpr auto SETUP_TIME = 10;

struct ThreadData {
  std::atomic<bool> keep_running{true};
  std::atomic<bool> count_events{false};
  std::atomic<uint64_t> event_count{0};
  std::vector<uint8_t>* message;
  char* server_addr;
  uint16_t port;
};

void writeMessage(int fd, std::vector<uint8_t>& msg)
{
  size_t written = 0;
  size_t n;
  while (written != msg.size()) {
    if ((n = write(fd, &msg[0] + written, msg.size() - written)) < 0) {
      perror("write()");
      exit(EXIT_FAILURE);
    } else {
      written += n;
    }
  }
}

void runThread(ThreadData& thread_data)
{
  // init socket
  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  inet_aton(thread_data.server_addr, &(address.sin_addr));
  address.sin_port = htons(thread_data.port);
  // connect to server
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (connect(sockfd, (struct sockaddr*)&address, sizeof(address)) == -1) {
    perror("connect()");
    exit(EXIT_FAILURE);
  }

  std::default_random_engine generator;
  std::exponential_distribution<double> distribution(POISSON_LAMBDA);
  std::atomic<int> packets_pending{0};
  std::vector<uint8_t> buf;

  std::cout << std::hex;

  // main loop
  while (thread_data.keep_running) {
    /*
    // don't flood the server indefinitely
    while (packets_pending > 100 && thread_data.keep_running) {
      pthread_yield();
    }
    */
    TPCC::tx(buf, 0);
    writeMessage(sockfd, buf);
    buf.clear();
    //    packets_pending++;

    // wait for an exponential distributed amount of time (poisson process)
    if (USE_POISSON) {
      int i = 1000 * distribution(generator);
      std::this_thread::sleep_for(std::chrono::milliseconds(i));
    }
  }

  close(sockfd);
}

int main(int argc, char* argv[])
{
  if (argc != 6) {
    std::cout << "Usage: " << argv[0] << " <ip address> <port> <number of threads> <testing time in s> <packet size in byte>" << std::endl;
    return 1;
  }

  ThreadData thread_data;
  uint thread_count;
  uint run_seconds;
  std::vector<uint8_t> message;

  try {
    thread_data.server_addr = argv[1];
    thread_data.port = std::stoi(argv[2]);
    thread_count = std::stoi(argv[3]);
    run_seconds = std::stoi(argv[4]);
    message.insert(message.begin(), std::stoi(argv[5]), 'a');
    thread_data.message = &message;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    std::cout << "Usage: " << argv[0] << " <ip address> <port> <number of threads> <testing time in s> <packet size in byte>" << std::endl;
    return 1;
  }

  // start threads
  std::vector<std::thread> threads;
  for (int t_i = 0; t_i < thread_count; t_i++) {
    threads.emplace_back(runThread, std::ref(thread_data));
  }

  for (;;) {
    sleep(10000);
  }

  /*
  sleep(SETUP_TIME);
  // start counting events
  thread_data.count_events = true;
  sleep(run_seconds);

  // stop threads
  thread_data.keep_running = false;
  for (auto& thread : threads) {
    thread.join();
  }

  std::cout << "events\tseconds\tevents/s\tms/event\n"
            << thread_data.event_count << "\t" << run_seconds << "\t" << (thread_data.event_count * 1.0) / run_seconds << "\t"
            << (run_seconds * 1000.0) / thread_data.event_count << "\t" << std::endl;
  */
  return 0;
}
