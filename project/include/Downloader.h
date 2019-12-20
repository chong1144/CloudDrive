#include "Package/Package.h"
#include "Config.h"
#include "Log.h"
#include "FileOperations.h"
#include <iostream>
#include <queue>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <ctime>
#include <map>
#include <unistd.h>
#include <sys/epoll.h>
#include <algorithm>
#include <set>

using std::make_pair;
using std::pair;
using std::set;
using std::to_string;
using size_t = uint64_t;
using pType = PackageType;
const size_t headlen = sizeof(UniformHeader);
using std::queue;
using FileHash = string;
using socket_t = int;

const int MAXEVENTS = 20;

class Downloader
{
    // 先发来的请求先记录在query队列中，等待数据库返回文件信息
	// 收到CTRL的上传请求添加，收到数据库发来的文件信息则删除
	queue<socket_t> queryQue;
    set<socket_t> allSet;
    set<socket_t> downloadSet;
    set<socket_t> idleSet;
    set<socket_t> pauseSet;

	map<socket_t, FileHash> sockFileMap;

    //database model to/from upload model
    int fifo_db_w;
    int fifo_db_r;

    //control model to/from upload model
    int fifo_ctrl_w;
    int fifo_ctrl_r;

    UniformHeader headPacket;
    DownloadFetchBody fetchPacket;
    DownloadDoneBody donePacket;
    DownloadPushBody pushPacket;
    DownloadReqBody downloadreqPacket;
    DownloadRespBody respPacket;
    
    FileInfoBody fileInfoPacket;

    Log fileLog;

    Config config;

    int epfd;
    epoll_event ep_events[MAXEVENTS];

    int socklisten;
    FileOperations filein;

    uint32_t port;
    string ip;
    uint8_t numListen;
    uint8_t maxEvent;
    sockaddr_in addr;

public:
    Downloader (const string& config_file);
    void init(const string&);
    void openfifo();
    void startServer();

    void EpollAdd(int fd, uint32_t events);

    void EpollDel(int fd);

    int EpollWait();

    int sendReqFileHashToDB();

    int sendPushToClient(const string& filehash, uint16_t chunkNo, socket_t sockclnt);

    void doneloadDone(const string&filehash);

    int run();

    int handleNewConnection(socket_t sockclnt);

    int handlePacketFromDB();

    int handlePacketFromCTRL();

    int handlePacketFromClient(socket_t sockclnt);

    int handleDownloadReq(socket_t sockclnt);

    int handleFetchReq(socket_t sockclnt);

    int handleDoneReq(socket_t sockclnt);

    int sendRespToClient(socket_t sockclnt);

    int sendReqToDB();



};