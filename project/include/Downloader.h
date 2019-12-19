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
    //database model to/from upload model
    int fifo_db_w;
    int fifo_db_r;

    //control model to/from upload model
    int fifo_ctrl_w;
    int fifo_ctrl_r;

    UniformHeader headPacket;
    DownloadPushBody pushPacket;
    DownloadReqBody downloadreqPacket;
    FileInfoBody fileinfoPacket;

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
    uint32_t sock;
    sockaddr_in addr;

public:
    Downloader (string)
};