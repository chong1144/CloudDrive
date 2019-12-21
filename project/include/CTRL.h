#pragma once

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <set>
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
using std::set;
using size_t = uint64_t;
using pType = PackageType;
const size_t headlen = sizeof (UniformHeader);
using FileHash = string;
using socket_t = int;
using session_t = string;

const int MAXEVENTS = 20;

class CTRL
{
    Log fileLog;
    uint32_t port;
    string ip;
    uint8_t numListen;
    sockaddr_in addr;

    uint8_t maxEvent;

    int fifo_ctod;
    int fifo_dtoc;

    int fifo_ctor;
    int fifo_rtoc;

    int fifo_ctos;
    int fifo_stoc;
	// 先发来的请求先记录在query队列中，等待数据库返回信息
	// 收到client请求添加，收到数据库发来的信息则删除
    queue<socket_t> queryQue;
    set<socket_t> allSet;
    map<socket_t, session_t> soseMap;
    map<session_t, socket_t> sesoMap;
    UniformHeader   headPacket;
    SigninBody      signinPacket;
    SigninresBody   signinresPacket;
    SignupBody      signupPacket;
    SignupresBody   signupresPacket;
    CopyBody        copyPacket;
    DeleteBody      deletePacket;
    MoveBody        movePacket;
    MkdirBody       mkdirPacket;
    SYNReqBody      synReqPacket;
    CopyRespBody        copyRespPacket;
    DeleteRespBody      deleteRespPacket;
    MkdirRespBody       mkdirRespPacket;
    MoveRespBody        moveRespPacket;
    SYNRespBody      synRespPacket;


    int epfd;
    // 先固定
    epoll_event ep_events[MAXEVENTS];
    int socklisten;

    


public:
    CTRL();
    CTRL(const string& configFile);
    ~CTRL();

    // 初始化 包括读取配置文件 创立管道
    void init(const string &configFile);
    // 启动 server 的 socket 绑定相关的端口 非阻塞模式
    void startServer();
    // 等待客户端的连接 并且分发不同的报头给不同的模块 非阻塞模式 死循环
    void waitForClient();

    void openfifo();

    int handleNewConnection(socket_t sockclnt);
    // 处理从数据库来的packet
    int handlePacketFromDB ();
    int handleSigninResFromDB();
    int handleSignupResFromDB();
    int handleCopyRespFromDB();
    int handleDeleteRespFromDB();
    int handleMkdirRespFromDB();
    int handleMoveRespFromDB();
    int handleSynRespFromDB();

    // 处理从Client来的packet
    int handlePacketFromClient(socket_t sockclnt);
    int handleSigninFromClient(socket_t sockclnt);
    int handleSignupFromClient(socket_t sockclnt);
    int handleCopyFromClient(socket_t sockclnt);
    int handleDeleteFromClient(socket_t sockclnt);
    int handleMkdirFromClient(socket_t sockclnt);
    int handleMoveFromClient(socket_t sockclnt);
    int handleSynReqFromClient(socket_t sockclnt);

    // 向Client发送回包
    int sendSignupResToClient(socket_t sockclnt);
    int sendSigninResToClient(socket_t sockclnt);
    int sendCopyRespToCilent(socket_t sockclnt);
    int sendDeleteRespToCilent(socket_t sockclnt);
    int sendMkdirRespToCilent(socket_t sockclnt);
    int sendMoveRespToCilent(socket_t sockclnt);
    int sendSynRespToCilent(socket_t sockclnt);
    // 向数据库发送请求
    int sendSigninToDB();
    int sendSignupToDB();
    int sendCopyToDB();
    int sendDeleteToDB();
    int sendMkdirToDB();
    int sendMoveToDB();
    int sendSynReqToDB();    
    // 创立管道
    bool createPipe(const string &pipe);
    // 添加 epoll 监视的句柄
	void EpollAdd (int fd, uint32_t events);

	int EpollWait ();

	void EpollDel (int fd);

    void handleDisconnection(socket_t);

    // 获取 socket 的 ip 和 端口
    bool GetIPPort(const uint32_t &sock, string &ip, uint16_t &port);
    
    void run();
};