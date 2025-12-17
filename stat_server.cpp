#include <array>
#include <atomic>
#include <cstdint>
#include <netinet/in.h>
#include <print>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <thread>
#include <unistd.h>
#include <vector>

std::atomic<int> id{0};

std::string get_info() {
  struct sysinfo info{};
  if (sysinfo(&info) != 0)
    return "<p>Failed to read system info</p>";

  return std::format("<p>Uptime: {} seconds</p>"
                     "<p>Total RAM: {} MB</p>"
                     "<p>Free RAM: {} MB</p>",
                     info.uptime, info.totalram / (1024 * 1024),
                     info.freeram / (1024 * 1024));
}

void make_http_response(std::string &body) {
  body.clear();
  body.reserve(512);

  body += "HTTP/1.1 200 OK\r\n";
  body += "Content-Type: text/html; charset=UTF-8\r\n";

  std::string html_body;
  html_body += "<!doctype html>";
  html_body += "<html>";
  html_body += "<head><title>Test</title></head>";
  html_body += "<body>";
  html_body += "<h1>System Info</h1>";
  html_body += get_info();
  html_body += "</body>";
  html_body += "</html>";

  body += "Content-Length: " + std::to_string(html_body.size()) + "\r\n";
  body += "Connection: close\r\n";
  body += "\r\n";
  body += html_body;
}

class server {
public:
  uint16_t _port{8080};
  int _socket{-1};
  sockaddr_in sockAddr{};
  std::vector<std::jthread> threads;
  std::array<char, 1024> buff{};
  std::string body;
  server() {
    if (_create_server() == 0)
      run();
  }

  server(uint16_t port) : _port(port) {
    if (_create_server() == 0)
      run();
  }

  void run() {
    while (!stop_source.stop_requested()) {
      int clientSocket = accept(_socket, nullptr, nullptr);
      if (clientSocket == -1)
        break;
      ssize_t n = recv(clientSocket, buff.data(), buff.size(), 0);
      if (n > 0) {
        std::println("Request has been sent");
        make_http_response(body);
        send(clientSocket, body.data(), body.size(), 0);
      }
      close(clientSocket);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      // threads.emplace_back([this, clientSocket](std::stop_token st) {
      //   handleClient(clientSocket, st);
      // });

      // threads.erase(
      //   std::remove_if(threads.begin(), threads.end(),
      //     [](const std::jthread& t) {
      //       return !t.joinable();
      //     }),
      //   threads.end()
      // );
    }
  }

  // void handleClient(int clientSocket, std::stop_token st) {
  //   std::array<char, 1024> buff{};
  //   ssize_t n = recv(clientSocket, buff.data(), buff.size(), 0);
  //   if (n > 0) {

  //     send(clientSocket, make_http_response().data(),
  //          make_http_response().size(), 0);
  //   }
  //   close(clientSocket);
  // }

  ~server() {
    std::println("We are closing :)");

    stop_source.request_stop();

    if (_socket != -1) {
      shutdown(_socket, SHUT_RDWR);
      close(_socket);
    }
    // no manual join needed — jthread does it automatically
  }

private:
  std::stop_source stop_source;

  int _create_server() {
    _socket = socket(AF_INET, SOCK_STREAM, 0);
    if (_socket == -1) {
      std::println("Failed to create socket");
      return 1;
    }

    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = INADDR_ANY;
    sockAddr.sin_port = htons(_port);

    if (bind(_socket, (sockaddr *)&sockAddr, sizeof(sockAddr)) == -1) {
      std::println("Failed to bind");
      return 1;
    }

    if (listen(_socket, 5) == -1) {
      std::println("listen() failed");
      return 1;
    }

    return 0;
  }
};

int main(int argc, char *argv[]) {
  if (argc >= 2) {
    uint16_t port = static_cast<uint16_t>(std::atoi(argv[1]));
    server srv(port);
  } else {
    server srv;
  }
}
