#pragma once

#include <string>
#include <vector>
#include <map>
#include <queue>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "Package/Package.h"
#include "Log.h"
#include "Config.h"

using std::map;
using std::pair;
using std::queue;
using std::string;
using std::to_string;
using std::vector;

// Session --- sock
using socketSession = map<string, uint32_t>;
using Sessionsocket = map<uint32_t, string>;

const uint16_t packetLength = 2048;

class control
{
private:
    Log l;
    uint32_t port;
    string ip;
    uint8_t numListen;

    uint32_t sock;
    sockaddr_in addr;

    uint8_t maxEvent;

    int fifo_ctod;
    int fifo_dtoc;

    int fifo_ctor;
    int fifo_rtoc;

    int fifo_ctos;
    int fifo_stoc;

public:
    control();
    control(const string &configFile);
    ~control();

    // 如果第一次传入配置文件的构造函数 不需要使用下列函数
    // 如果使用无参构造函数构造改类 需要依次使用下列函数

    // 初始化 包括读取配置文件 创立管道
    void init(const string &configFile);
    // 启动 server 的 socket 绑定相关的端口 非阻塞模式
    void startServer();
    // 等待客户端的连接 并且分发不同的报头给不同的模块 非阻塞模式 死循环
    void waitForClient();


    // 一些工具函数

    // 创立管道
    bool createPipe(const string &pipe);
    // 添加 epoll 监视的句柄
    void EpollAdd(const int &epfd, const int &fd, const uint32_t &events);
    // epoll 等待函数
    int EpollWait(const int &epfd, epoll_event* ep_events);
    // epoll 去除句柄
    void EpollDel(const int &epfd, const int &fd);
    // 获取 socket 的 ip 和 端口
    bool GetIPPort(const uint32_t &sock, string &ip, uint16_t &port);
};

struct ControlBufferUnit
{
    UniformHeader header;
    void * body;
};
