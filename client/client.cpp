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

#include "PacketProtocol.hpp"

constexpr auto USE_POISSON = false;
constexpr auto POISSON_LAMBDA = 3.5;
constexpr auto SETUP_TIME = 10;

struct ThreadData {
  std::atomic<bool> keep_running{true};
  std::atomic<bool> count_events{false};
  std::atomic<uint64_t> event_count{0};
  char* server_addr;
  uint16_t port;
};

void msgHandler(int fd, std::vector<uint8_t>& data)
{
  // std::cout << "recv(" << fd << "): " << std::string(data.begin(), data.end()) << "\n";
  // send random number
  std::string s = std::to_string(rand());
  auto msg = Net::wrapMessage(reinterpret_cast<const uint8_t*>(s.c_str()), s.length());
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

  uint8_t buf[100];
  uint64_t local_ev_count = 0;
  std::default_random_engine generator;
  std::exponential_distribution<double> distribution(POISSON_LAMBDA);

  Net::PacketProtocol packetizer([&](auto& data) {
    // count events
    if (thread_data.count_events)
      local_ev_count++;

    // wait for an exponential distributed amount of time (poisson process)
    if (USE_POISSON) {
      int i = 1000 * distribution(generator);
      std::this_thread::sleep_for(std::chrono::milliseconds(i));
    }

    msgHandler(sockfd, data);
  });

  // first message
  buf[0] = 0;
  auto msg = Net::wrapMessage(buf, 1);
  if (write(sockfd, &msg[0], msg.size()) < 0) {
    perror("write()");
    exit(EXIT_FAILURE);
  }

  // main loop
  while (thread_data.keep_running) {
    // wait for data and read in
    int n = read(sockfd, buf, sizeof(buf));
    if (n < 0) {
      perror("read()");
      exit(EXIT_FAILURE);
    }
    packetizer.receive(buf, n);
  }

  close(sockfd);
  thread_data.event_count += local_ev_count;
}

int main(int argc, char* argv[])
{
  if (argc != 5) {
    std::cout << "Usage: " << argv[0] << " <ip address> <port> <number of threads> <testing time in s>" << std::endl;
    return 1;
  }

  ThreadData thread_data;
  thread_data.server_addr = argv[1];
  thread_data.port = std::stoi(argv[2]);

  uint thread_count;
  uint run_seconds;

  try {
    thread_count = std::stoi(argv[3]);
    run_seconds = std::stoi(argv[4]);
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    std::cout << "Usage: " << argv[0] << " <ip address> <port> <number of threads> <testing time in s>" << std::endl;
    return 1;
  }

  // start threads
  std::vector<std::thread> threads;
  for (int t_i = 0; t_i < thread_count; t_i++) {
    threads.emplace_back(runThread, std::ref(thread_data));
  }

  sleep(SETUP_TIME);
  // start counting events
  thread_data.count_events = true;
  sleep(run_seconds);

  // stop threads
  thread_data.keep_running = false;
  for (auto& thread : threads) {
    thread.join();
  }

  std::cout << thread_data.event_count << " events in " << run_seconds << "s\n"
            << (thread_data.event_count * 1.0) / run_seconds << " events per second\n"
            << (run_seconds * 1000.0) / thread_data.event_count << " ms per event" << std::endl;

  return 0;
}
