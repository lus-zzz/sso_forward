#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include <WS2tcpip.h>
#include <winsock2.h>

#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "hv/UdpClient.h"
#include "hv/UdpServer.h"
#include "hv/htime.h"
#include "hv/hbase.h"
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
  return 0;
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

#include <chrono>
#include <sstream>
#include <string>

std::string now_time() {
  // 获取当前系统时间
  auto now = std::chrono::system_clock::now();

  // 获取当前系统时间的时间点结构
  std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

  // 将时间点转换为本地时间
  std::tm* timeInfo = std::localtime(&currentTime);

  // 输出中国时区时间
  std::ostringstream oss;
  oss << std::put_time(timeInfo, "%Y-%m-%d %H:%M:%S");
  std::string timeString = oss.str();

  return timeString;
}

std::string create_sql = R"(
  DROP TABLE IF EXISTS "iot_config";
  CREATE TABLE "iot_config" (
    "camera_host" text NOT NULL,
    "audio_host" text NOT NULL,
    "audio_name" text NOT NULL,
    "static_electricity_host" text NOT NULL,
    PRIMARY KEY ("camera_host")
  );  
  DROP TABLE IF EXISTS "iot_status";
  CREATE TABLE "iot_status" (
    "host" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "touch_time" text NOT NULL,
    "heartbeat_time" text NOT NULL,
    PRIMARY KEY ("host")
  );
  DROP TABLE IF EXISTS "touch_log";
  CREATE TABLE "touch_log" (
    "host" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "touch_time" text NOT NULL,
    "id" INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT
  );
)";

void update_time(SQLite::Database& database, std::string host, std::string name,
                 bool is_touch) {
  try {
    // 开启事务
    SQLite::Transaction transaction(database);

    // 检查是否存在对应的host记录
    SQLite::Statement query(database,
                            "SELECT COUNT(*) FROM iot_status WHERE host = ?");
    query.bind(1, host);

    int count = 0;
    if (query.executeStep()) {
      count = query.getColumn(0).getInt();
    }

    if (count > 0) {
      // host存在，执行更新操作
      if (is_touch) {
        SQLite::Statement updateQuery(database,
                                      "UPDATE iot_status SET touch_time = ? "
                                      ",heartbeat_time = ? WHERE host = ?");
        updateQuery.bind(1, now_time());
        updateQuery.bind(2, now_time());
        updateQuery.bind(3, host);
        updateQuery.exec();

      } else {
        SQLite::Statement updateQuery(
            database,
            "UPDATE iot_status SET heartbeat_time = ? WHERE host = ?");
        updateQuery.bind(1, now_time());
        updateQuery.bind(2, host);
        updateQuery.exec();
      }
    } else {
      // host不存在，执行插入操作
      SQLite::Statement insertQuery(database,
                                    "INSERT INTO iot_status (host, name, "
                                    "touch_time, heartbeat_time) VALUES "
                                    "(?, ?, ?, ?)");
      insertQuery.bind(1, host);
      insertQuery.bind(2, name);
      is_touch ? insertQuery.bind(3, now_time()) : insertQuery.bind(3, "");
      insertQuery.bind(4, now_time());
      insertQuery.exec();
    }
    if (is_touch) {
      SQLite::Statement insertQuery(database,
                                    "INSERT INTO touch_log (host, name, "
                                    "touch_time) VALUES "
                                    "(?, ?, ?)");
      insertQuery.bind(1, host);
      insertQuery.bind(2, name);
      insertQuery.bind(3, now_time());
      insertQuery.exec();
    }

    // 提交事务
    transaction.commit();
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

std::string search_audio_name(SQLite::Database& database,
                              std::string static_electricity_host) {
  std::string audioName = "";
  try {
    // Prepare the query
    SQLite::Statement query(
        database,
        "SELECT audio_name FROM iot_config WHERE static_electricity_host = ?");

    // Bind the static_electricity_host value
    std::string staticElectricityHost = static_electricity_host;
    query.bind(1, staticElectricityHost);

    // Execute the query and retrieve the audio_name
    if (query.executeStep()) {
      audioName = query.getColumn(0).getString();
      std::cout << "Audio Name: " << audioName << std::endl;
    } else {
      std::cout << "No matching records found." << std::endl;
    }
  } catch (const std::exception& e) {
    std::cerr << "SQLite error: " << e.what() << std::endl;
  }
  return audioName;
}

unsigned char calculateChecksum(const char* data, size_t length) {
  unsigned char checksum = 0;

  // 逐个取出数据并进行累加
  for (size_t i = 0; i < length; ++i) {
    checksum += static_cast<unsigned char>(data[i]);
  }

  // 取累加和的低字节作为校验码
  return checksum & 0xFF;
}

std::string utf8_to_gbk(const std::string& utf8_str) {
  // 获得转换所需的缓冲区大小
  int len = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, NULL, 0);
  wchar_t* wide_str = new wchar_t[len];
  MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, wide_str, len);

  // 获得转换后的 GBK 编码字符串所需的缓冲区大小
  len = WideCharToMultiByte(CP_ACP, 0, wide_str, -1, NULL, 0, NULL, NULL);
  char* gbk_str = new char[len];
  WideCharToMultiByte(CP_ACP, 0, wide_str, -1, gbk_str, len, NULL, NULL);

  std::string result(gbk_str);

  delete[] wide_str;
  delete[] gbk_str;

  return result;
}

int main(int argc, char* argv[]) {
  // 指定数据库文件的路径
  const std::string databasePath =
      "D:/work/video_analysis_system/sqlite/database.db";

  // 检查数据库文件是否存在
  bool databaseExists = std::filesystem::exists(databasePath);

  // 创建或打开数据库
  SQLite::Database database(databasePath,
                            SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

  if (!databaseExists) {
    // 数据库文件不存在，执行创建表的操作
    database.exec(create_sql);
  }
#if 1
  std::vector<ConfInfo> conf = read_conf("config.txt");

  int port = 2001;

  UdpServer srv;
  int bindfd = srv.createsocket(port);
  if (bindfd < 0) {
    return -20;
  }
  printf("server bind on port %d, bindfd=%d ...\n", port, bindfd);
  srv.onMessage = [&](const SocketChannelPtr& channel, Buffer* buf) {
    // echo
    // printf("*** RX %s ***\n",channel->peeraddr().c_str());
    // hexdump((char*)buf->data(), (int)buf->size());
    // printf("**********\n\n");
    std::string src_host = host_split(channel->peeraddr(), ':').at(0);
    unsigned char* data = (unsigned char*)buf->data();
    if (data[12] == 0xA0) {
      printf("*** ER %s ***\n", channel->peeraddr().c_str());
      hexdump((char*)buf->data(), (int)buf->size());
      update_time(database, src_host, "静电释放仪", true);
      char exe_dir[256] = {0};
      std::string play_cmd = "MP3_PLAY;";
      play_cmd.append(search_audio_name(database,src_host));
      play_cmd.append(get_executable_dir(exe_dir, 256));
      play_cmd.append("/静电以释放.mp3;50;A;100;15;");
      std::cout << play_cmd << std::endl;
      // char play_cmd[] = "MP3_PLAY;测试音响,1234;25;静电以释放.mp3;50;A;100;15;";
      std::string gbk_str = utf8_to_gbk(play_cmd);
      send_udp("0.0.0.0","127.0.0.1", 51201, gbk_str.c_str(), gbk_str.size());
      char send_data[17];
      memcpy(send_data, data, 14);
      send_data[11] = 0x02;
      send_data[14] = calculateChecksum(send_data, 14);
      send_data[15] = 0xF8;
      send_data[16] = 0x8F;
      channel.get()->write(send_data, 17);
      for (ConfInfo s : conf) {
        if (src_host == s.src_host) {
          std::cout << s.src_host << "," << s.bind_host << "," << s.dst_host
                    << "," << s.dst_port << std::endl;
          send_udp(s.bind_host.c_str(), s.dst_host.c_str(),
                   std::stoi(s.dst_port), (char*)buf->data(), buf->size());
        }
      }
    }
    if (data[12] == 0xA1) {
      update_time(database, src_host, "静电释放仪", false);
    }
  };
  srv.onWriteComplete = [](const SocketChannelPtr& channel, Buffer* buf) {
    hexdump((char*)buf->data(), (int)buf->size());
    printf("**********\n\n");
    // printf("> %.*s\n", (int)buf->size(), (char*)buf->data());
  };
  srv.start();

  // press Enter to stop
  while (getchar() != '\n')
    ;
#endif
  return 0;
}
