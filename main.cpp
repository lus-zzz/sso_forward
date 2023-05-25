#include <WS2tcpip.h>
#include <winsock2.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#include "hv/UdpServer.h"
#include "hv/htime.h"
using namespace hv;

#pragma comment(lib, "ws2_32.lib")  // 链接到 ws2_32.lib 库文件

int send_udp(const char* bind_addr, const char* to_addr, int to_port,
             const char* sendbuf, int bufsize) {
  // 初始化 Winsock 库
  WSADATA wsaData;
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    std::cerr << "WSAStartup failed with error: " << iResult << std::endl;
    return 1;
  }

  // 创建一个 UDP socket
  SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET) {
    std::cerr << "socket failed with error: " << WSAGetLastError() << std::endl;
    WSACleanup();
    return 1;
  }

  // 绑定到指定IP和端口
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(0);                       // 选择要绑定的端口
  inet_pton(AF_INET, bind_addr, &addr.sin_addr);  // 选择要绑定的IP地址
  iResult = bind(sock, (SOCKADDR*)&addr, sizeof(addr));
  if (iResult == SOCKET_ERROR) {
    std::cerr << "bind failed with error: " << WSAGetLastError() << std::endl;
    closesocket(sock);
    WSACleanup();
    return 1;
  }

  // 发送数据
  // const char *sendbuf = "Hello, world!";
  sockaddr_in dest_addr;
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(to_port);               // 选择要发送的端口
  inet_pton(AF_INET, to_addr, &dest_addr.sin_addr);  // 选择要发送的IP地址
  iResult = sendto(sock, sendbuf, bufsize, 0, (SOCKADDR*)&dest_addr,
                   sizeof(dest_addr));
  if (iResult == SOCKET_ERROR) {
    std::cerr << "sendto failed with error: " << WSAGetLastError() << std::endl;
    closesocket(sock);
    WSACleanup();
    return 1;
  }

  // 关闭 socket
  closesocket(sock);

  // 释放 Winsock 库
  WSACleanup();
}

struct ConfInfo {
  std::string src_host;
  std::string bind_host;
  std::string dst_host;
  std::string dst_port;
};

std::vector<ConfInfo> read_conf(std::string file_name) {
  std::ifstream file(file_name);
  std::string line;
  std::vector<ConfInfo> conf_infos;

  while (std::getline(file, line)) {
    std::stringstream ss(line);
    std::string token;
    std::vector<std::string> tokens;
    while (std::getline(ss, token, ',')) {
      tokens.push_back(token);
    }

    ConfInfo conf_info;
    if (tokens.size() == 4) {
      conf_info.src_host = tokens[0];
      conf_info.bind_host = tokens[1];
      conf_info.dst_host = tokens[2];
      conf_info.dst_port = tokens[3];
      conf_infos.push_back(conf_info);
    }
  }

  for (ConfInfo s : conf_infos) {
    std::cout << s.src_host << "," << s.bind_host << "," << s.dst_host << ","
              << s.dst_port << std::endl;
  }
  return conf_infos;
}
#include <iomanip>
void hexdump(const void* data, size_t size) {
  const unsigned char* p = reinterpret_cast<const unsigned char*>(data);
  size_t i = 0;
  while (i < size) {
    std::cout << std::hex << std::setfill('0') << std::setw(2)
              << static_cast<int>(*p++);
    if (++i % 21 == 0)
      std::cout << '\n';
    else
      std::cout << ' ';
  }
  if (i % 21 != 0) std::cout << '\n';
}
#include <sstream>
std::vector<std::string> host_split(const std::string& str, char delimiter) {
  std::vector<std::string> tokens;
  std::stringstream ss(str);
  std::string token;
  while (getline(ss, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

int main(int argc, char* argv[]) {
  std::vector<ConfInfo> conf = read_conf("config.txt");

  int port = 2001;

  UdpServer srv;
  int bindfd = srv.createsocket(port);
  if (bindfd < 0) {
    return -20;
  }
  printf("server bind on port %d, bindfd=%d ...\n", port, bindfd);
  srv.onMessage = [conf](const SocketChannelPtr& channel, Buffer* buf) {
    // echo
    // printf("*** RX %s ***\n",channel->peeraddr().c_str());
    // hexdump((char*)buf->data(), (int)buf->size());
    // printf("**********\n\n");
    unsigned char* data = (unsigned char*)buf->data();
    if (data[12] == 0xA0) {
      printf("*** ER %s ***\n", channel->peeraddr().c_str());
      hexdump((char*)buf->data(), (int)buf->size());
      printf("**********\n\n");
      std::string src_host = host_split(channel->peeraddr(), ':').at(0);
      for (ConfInfo s : conf) {
        if (src_host == s.src_host) {
          std::cout << s.src_host << "," << s.bind_host << "," << s.dst_host
                    << "," << s.dst_port << std::endl;
          send_udp(s.bind_host.c_str(), s.dst_host.c_str(),
                   std::stoi(s.dst_port), (char*)buf->data(), buf->size());
        }
      }
    }
  };
  srv.onWriteComplete = [](const SocketChannelPtr& channel, Buffer* buf) {
    printf("> %.*s\n", (int)buf->size(), (char*)buf->data());
  };
  srv.start();

  // press Enter to stop
  while (getchar() != '\n')
    ;
  return 0;
}
