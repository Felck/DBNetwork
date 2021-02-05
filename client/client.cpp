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

constexpr auto USE_POISSON = false;
constexpr auto POISSON_LAMBDA = 3.5;
constexpr auto SETUP_TIME = 10;

std::vector<int> openDeadConnections(int n, const char* server_addr, uint16_t port)
{
  std::vector<int> sockets;
  sockets.reserve(n);
  int sockfd;
  struct sockaddr_in address;
  for (size_t i = 0; i < n; i++) {
    // init socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_aton(server_addr, &(address.sin_addr));
    address.sin_port = htons(port);
    // connect to server
    if (connect(sockfd, (struct sockaddr*)&address, sizeof(address)) == -1) {
      perror("connect()");
      std::cout << sockfd << " " << errno << std::endl;
      exit(EXIT_FAILURE);
    }
    sockets.push_back(sockfd);
  }
  return sockets;
}

void runThread(const std::atomic<bool>& keep_running,
               const std::atomic<bool>& count_events,
               std::atomic<uint64_t>& ev_count,
               const char* server_addr,
               uint16_t port)
{
  // init socket
  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  inet_aton(server_addr, &(address.sin_addr));
  address.sin_port = htons(port);
  // connect to server
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (connect(sockfd, (struct sockaddr*)&address, sizeof(address)) == -1) {
    perror("connect()");
    exit(EXIT_FAILURE);
  }

  char buf[100];
  uint64_t local_ev_count = 0;
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
      std::cerr << "Received wrong response! (" << s << "/" << buf << ")" << std::endl;
      exit(EXIT_FAILURE);
    }

    if (count_events)
      local_ev_count++;

    if (USE_POISSON) {
      // wait for an exponential distributed amount of time (poisson process)
      i = 1000 * distribution(generator);
      std::this_thread::sleep_for(std::chrono::milliseconds(i));
    }
  }

  close(sockfd);
  ev_count += local_ev_count;
}

int main(int argc, char* argv[])
{
  if (argc != 6) {
    std::cout << "Usage: " << argv[0] << " <ip address> <port> <number of threads> <number of dead connections> <testing time in s>" << std::endl;
    return 1;
  }

  uint thread_count;
  uint dead_conns;
  uint run_seconds;
  char* server_addr = argv[1];
  uint16_t port = std::stoi(argv[2]);

  try {
    thread_count = std::stoi(argv[3]);
    dead_conns = std::stoi(argv[4]);
    run_seconds = std::stoi(argv[5]);
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    std::cout << "Usage: " << argv[0] << " <ip address> <port> <number of threads> <number of dead connections> <testing time in s>" << std::endl;
    return 1;
  }

  auto dead_conn_sockets = openDeadConnections(dead_conns, server_addr, port);

  // start threads
  std::atomic<bool> keep_running{true};
  std::atomic<bool> count_events{false};
  std::atomic<uint64_t> event_count{0};
  std::vector<std::thread> threads;
  for (int t_i = 0; t_i < thread_count; t_i++) {
    threads.emplace_back([&]() { runThread(keep_running, count_events, event_count, server_addr, port); });
  }

  sleep(SETUP_TIME);
  // start counting events
  count_events = true;
  sleep(run_seconds);

  // stop threads
  keep_running = false;
  for (auto& thread : threads) {
    thread.join();
  }

  std::cout << event_count << " events in " << run_seconds << "s\n"
            << (event_count * 1.0) / run_seconds << " events per second\n"
            << (run_seconds * 1000.0) / event_count << " ms per event" << std::endl;

  // close dead connection sockets
  for (auto socket : dead_conn_sockets)
    close(socket);

  return 0;
}
