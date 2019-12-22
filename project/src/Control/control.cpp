#include "control.h"

control::control()
{
}

control::control(const string &configFile)
{
    this->init(configFile);

    this->startServer();

    this->waitForClient();
}

control::~control()
{
    // 关闭管道
    close(this->fifo_ctod);
    close(this->fifo_dtoc);

    close(this->fifo_ctor);
    close(this->fifo_rtoc);

    close(this->fifo_ctos);
    close(this->fifo_stoc);
}

bool control::createPipe(const string &pipe)
{
    umask(0);
    if (mkfifo(pipe.c_str(), S_IFIFO | 0666) == -1)
    {
        string ErrorContent = "Fail to create pipe " + pipe;
        l.writeLog(Log::WARNING, ErrorContent);
        return false;
    }

    string InfoContent = "create pipe " + pipe + " successfully!";
    l.writeLog(Log::INFO, InfoContent);
    return true;
}

void control::init(const string &configFile)
{
    // 读取配置文件
    Config c(configFile);

    // 初始化 socket需要用的参数
    port = atoi(c.getValue("Control", "port").c_str());
    ip = c.getValue("Control", "ip");
    numListen = atoi(c.getValue("Control", "numListen").c_str());
    maxEvent = atoi(c.getValue("Control", "maxEvent").c_str()) + 2;

    // 初始化日志需要用的参数
    l.init(c.getValue("Control", "controlLogPosition"));

    unlink(c.getValue("Global", "path_fifo_ctod").c_str());
    unlink(c.getValue("Global", "path_fifo_dtoc").c_str());
    unlink(c.getValue("Global", "path_fifo_stod").c_str());
    unlink(c.getValue("Global", "path_fifo_dtos").c_str());
    unlink(c.getValue("Global", "path_fifo_rtod").c_str());
    unlink(c.getValue("Global", "path_fifo_dtor").c_str());

    unlink(c.getValue("Global", "path_fifo_ctor").c_str());
    unlink(c.getValue("Global", "path_fifo_rtoc").c_str());
    unlink(c.getValue("Global", "path_fifo_ctos").c_str());
    unlink(c.getValue("Global", "path_fifo_stoc").c_str());

    // 初始化管道的参数并且创建管道
    if (!createPipe(c.getValue("Global", "path_fifo_ctod")))
        ;
    if (!createPipe(c.getValue("Global", "path_fifo_dtoc")))
        ;
    if (!createPipe(c.getValue("Global", "path_fifo_stod")))
        ;
    if (!createPipe(c.getValue("Global", "path_fifo_dtos")))
        ;
    if (!createPipe(c.getValue("Global", "path_fifo_rtod")))
        ;
    if (!createPipe(c.getValue("Global", "path_fifo_dtor")))
        ;
    if (!createPipe(c.getValue("Global", "path_fifo_ctor")))
        ;
    if (!createPipe(c.getValue("Global", "path_fifo_rtoc")))
        ;
    if (!createPipe(c.getValue("Global", "path_fifo_ctos")))
        ;
    if (!createPipe(c.getValue("Global", "path_fifo_stoc")))
        ;

    l.writeLog(Log::INFO, "create pipes successfully!");

    // 打开管道 没有检查是否打开成功 关闭一些目前不需要的通道
    this->fifo_ctod = open(c.getValue("Global", "path_fifo_ctod").c_str(), O_WRONLY);
    this->fifo_dtoc = open(c.getValue("Global", "path_fifo_dtoc").c_str(), O_RDONLY);
    l.writeLog(Log::INFO, "open ipc with database successfully!");

    // this->fifo_ctos = open(c.getValue("Global", "path_fifo_ctos").c_str(), O_WRONLY);
    // this->fifo_stoc = open(c.getValue("Global", "path_fifo_stoc").c_str(), O_RDONLY);
    // l.writeLog(Log::INFO, "open ipc with uploader successfully!");

    this->fifo_rtoc = open(c.getValue("Global", "path_fifo_rtoc").c_str(), O_RDONLY);
    this->fifo_ctor = open(c.getValue("Global", "path_fifo_ctor").c_str(), O_WRONLY);
    l.writeLog(Log::INFO, "open ipc downloader successfully!");

    l.writeLog(Log::INFO, "control read config file successfully!");
}

void control::startServer()
{
    // 建立起连接 采用非阻塞的方式进行一切的操作
    int ret = 0;

    // 创建socket 设置非阻塞
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        l.writeLog(Log::ERROR, "Fail to create server socket");
        exit(-1);
    }
    else if(0)
    {
        int flag = fcntl(sock, F_GETFL);
        fcntl(sock, flag | O_NONBLOCK);
    }

    // 设置地址复用等
    int opt = 1;
    ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    if (ret == -1)
    {
        l.writeLog(Log::ERROR, "Fail to setsockopt");
        exit(-1);
    }

    // 设置地址等连接参数
    if (inet_pton(AF_INET, this->ip.c_str(), &addr.sin_addr) <= 0)
    {
        l.writeLog(Log::ERROR, "invalid address");
        exit(-1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(this->port);

    // 绑定地址
    ret = bind(sock, (sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
    {
        l.writeLog(Log::ERROR, "bind error");
        exit(-1);
    }

    ret = listen(sock, numListen);
    if (ret < 0)
    {
        l.writeLog(Log::ERROR, "listen error");
        exit(-1);
    }

    string InfoContent = "Start Server at " + ip + ":" + to_string(port);
    InfoContent += " with mutiple " + to_string(numListen) + " listeners";
    l.writeLog(Log::INFO, InfoContent);
}

void control::waitForClient()
{
    size_t addrlen = sizeof(this->addr);

    // 创立 epoll 句柄
    int epfd = epoll_create1(0);
    if (epfd == -1)
    {
        l.writeLog(Log::ERROR, "Fail to create epoll fd");
        exit(-1);
    }

    // 将 sock 加入 epoll 监视中
    this->EpollAdd(epfd, sock, EPOLLIN);

    // 将管道加入 epoll 监视
    this->EpollAdd(epfd, this->fifo_dtoc, EPOLLIN);
    // ????

    // 申请 epoll_event*
    epoll_event *ep_events = new epoll_event[maxEvent];
    if (ep_events == NULL)
    {
        string ErrorContent = "Fail to apply " + to_string(maxEvent) + " epoll_event";
        l.writeLog(Log::ERROR, ErrorContent);
        exit(-1);
    }

    int ret;
    int candidateSock;
    uint32_t candidateSockEvents;

    queue<uint32_t> sockQueue;
    socketSession sS;
    Sessionsocket Ss;
    UniformHeader u;
    int readres;
    char *packet = new char[packetLength];

    queue<ControlBufferUnit> BufferQueue;
    ControlBufferUnit cu;
    while (1)
    {
        ret = this->EpollWait(epfd, ep_events);
        if (ret == -1)
        {
            l.writeLog(Log::ERROR, "epoll wait error");
            exit(-1);
        }
        // l.writeLog(Log::INFO, "Here for you");

        for (size_t i = 0; i < ret; i++)
        {
            candidateSock = ep_events[i].data.fd;
            candidateSockEvents = ep_events[i].events;
            if (candidateSock == sock)
            {
                // 有新连接来临时
                if (candidateSockEvents & EPOLLIN)
                {
                    int accres;
                    accres = accept(sock, (sockaddr *)&this->addr, (socklen_t *)&addrlen);
                    if (accres <= 0)
                    {
                        l.writeLog(Log::ERROR, "accept error");
                        exit(-1);
                    }
                    // 设置非阻塞的方法 禁用目前
                    if (0)
                    {
                        int flag = fcntl(accres, F_GETFL);
                        fcntl(accres, flag | O_NONBLOCK);
                    }

                    this->EpollAdd(epfd, accres, EPOLLIN | EPOLLOUT);
                    string ip;
                    uint16_t port;
                    GetIPPort(accres, ip, port);
                    string InfoContent = "Connect with " + ip + ":" + to_string(port) + " and Socket is " + to_string(accres);
                    l.writeLog(Log::INFO, InfoContent);

                    // sockQueue.push(accres);
                }
            }
            else if (candidateSock == this->fifo_dtoc && candidateSockEvents & EPOLLIN)
            {
                // 数据库要写东西给我
                readres = read(this->fifo_dtoc, &u, sizeof(u));
                if (readres <= 0)
                {
                    l.writeLog(Log::ERROR, "connect with database failed");
                    exit(-1);
                }
                else if (readres != sizeof(u))
                {
                    // 罕见情况
                    l.writeLog(Log::WARNING, "rare situation");
                    break;
                }
                readres = read(this->fifo_dtoc, packet, u.len);
                if (readres <= 0)
                {
                    l.writeLog(Log::ERROR, "connect with database failed");
                    exit(-1);
                }
                else if (readres != u.len)
                {
                    // 罕见情况
                    l.writeLog(Log::WARNING, "rare situation");
                    break;
                }

                // 获取每一个包的 Session
                l.writeLog(Log::INFO, ((SigninresBody *)packet)->Session); 
                string Session(((SigninresBody *)packet)->Session);
                l.writeLog(Log::INFO, Session); 
                // memcpy(Session, packet, SessionLength);

                // 建立 Session --- sock 映射表
                if (u.p == PackageType::SIGNIN_RES || u.p == PackageType::SIGNUP_RES)
                {
                    int *Code = (int *)&packet[SessionLength];
                    if (*Code == 0)
                    {
                        string InfoContent = to_string(sockQueue.front()) + "'s Session get " + Session;
                        l.writeLog(Log::INFO, InfoContent);
                        sS[Session] = sockQueue.front();
                        Ss[sockQueue.front()] = Session;
                        sockQueue.pop();
                    }
                    else 
                    {
                        l.writeLog(Log::WARNING, "Signin or Signup failed");
                    }
                }

                cu.header = u;
                cu.body = (void *)new char[u.len];
                memcpy(cu.body, packet, u.len);

                BufferQueue.push(cu);
            }
            else if (candidateSockEvents & EPOLLHUP | candidateSockEvents & EPOLLERR)
            {
                EpollDel(epfd, candidateSock);

                l.writeLog(Log::WARNING, "Delete Epollhup socket");
            }
            else if (candidateSockEvents & EPOLLIN)
            {
                // 收到新的内容
                readres = read(candidateSock, &u, sizeof(u));
                if (readres <= 0)
                {
                    string ip;
                    uint16_t port;
                    GetIPPort(candidateSock, ip, port);
                    string ErrorContent = "disconnect with client " + ip + ":" + to_string(port);
                    l.writeLog(Log::ERROR, ErrorContent);
                    sS.erase(Ss.at(candidateSock));
                    Ss.erase(candidateSock);
                    EpollDel(epfd, candidateSock);
                }
                else if (readres != sizeof(u))
                {
                    // 罕见情况
                    l.writeLog(Log::WARNING, "rare situation");
                }
                l.writeLog(Log::INFO, string("read uhead succ!!"));
                string InfoContent;

                switch (u.p)
                {
                // // for r
                // case PackageType::UPLOAD_REQ:
                //     write(this->fifo_ctor, &u, sizeof(u));
                //     write(this->fifo_ctor, &candidateSock, sizeof(candidateSock));
                //     this->EpollDel(epfd, candidateSock);

                //     break;

                // // for s
                // case PackageType::DOWNLOAD_REQ:
                //     write(this->fifo_ctos, &u, sizeof(u));
                //     write(this->fifo_ctos, &candidateSock, sizeof(candidateSock));
                //     this->EpollDel(epfd, candidateSock);

                //     break;

                // for d
                case PackageType::SIGNIN:
                case PackageType::SIGNUP:
                case PackageType::SYN_REQ:
                case PackageType::COPY:
                case PackageType::MOVE:
                case PackageType::DELETE:
                case PackageType::MKDIR:
                    //InfoContent = to_string(u.p) + " : " + to_string(u.len);
                    //l.writeLog(Log::INFO, InfoContent);
                    write(this->fifo_ctod, &u, sizeof(u));
                    sockQueue.push(candidateSock);  //将向数据库发起请求的sock暂存，数据库来包时匹配队首
                    readres = read(candidateSock, packet, u.len);
                    if (readres != u.len)
                    {
                        // 罕见情况
                        l.writeLog(Log::WARNING, "rare situation");
                        exit(-1);
                    }

                    write(this->fifo_ctod, packet, u.len);

                    if (u.p == PackageType::SIGNUP)
                    {
                        SignupBody *b = (SignupBody *)packet;
                        InfoContent.clear();
                        InfoContent = string("ip:") + b->IP + " username:" + b->Username + "psw:" + b->Password;
                        l.writeLog(Log::INFO, InfoContent);
                    }
                    break;

                default:
                    l.writeLog(Log::WARNING, "EPOLLHup socket rubbish");
                    break;
                }
            }
            else if (candidateSockEvents & EPOLLOUT)
            {
                if (BufferQueue.empty())
                    continue;

                cu = BufferQueue.front();

                if(cu.header.p == SIGNIN_RES || cu.header.p == SIGNUP_RES)
                {
                    int *Code = (int *)&(((char *)cu.body)[SessionLength]);
                    l.writeLog(Log::INFO, string("Codes: " ) + to_string(*Code));
                    if(*Code != 0)
                    {
                        int temp = sockQueue.front();
                        if(sockQueue.empty())
                        {
                            l.writeLog(Log::INFO, string("EMPTY"));
                            continue;
                        }
                        l.writeLog(Log::INFO, string("TEMP: " + to_string(temp)));
                        write(temp, &cu.header, sizeof(cu.header));
                        write(temp, cu.body, cu.header.len);
                        // delete (char *)cu.body;

                        string ip;
                        uint16_t port;
                        GetIPPort(temp, ip, port);
                        string WARContent = "Fail to signup or signin with client " + ip + ":" + to_string(port); 
                        l.writeLog(Log::WARNING, WARContent);
                        
                        sockQueue.pop();
                        EpollDel(epfd, temp);
                        // close(temp);
                        shutdown(temp, SHUT_RDWR);
                        
                        continue;
                    }

                }

                if(cu.header.p == PackageType::SYN_PUSH)
                {
                    SYNPushBody* spb = (SYNPushBody *)cu.body;
                    l.writeLog(Log::INFO, to_string(cu.header.len) + "Session: " 
                    + spb->Session + " Name: " + spb->name + " isFile: " + to_string(spb->isFile) + " id: " + to_string(spb->id));

                }
                string Session((char *)cu.body);
                l.writeLog(Log::INFO, string("Session: ") + Session);
                if(sS.find(Session) == sS.end())
                {
                    l.writeLog(Log::ERROR, "Sign in First!!!!");
                    continue;
                }
                // 传给 Client 端
                write(sS.at(Session), &cu.header, sizeof(u));
                write(sS.at(Session), cu.body, u.len);
                l.writeLog(Log::INFO, string("session:")+Session+" socket:"+ to_string(sS.at(Session)));
                // delete (char *)cu.body;
                
                BufferQueue.pop();
                l.writeLog(Log::INFO, "Write Success packageType:"+to_string(cu.header.p));
            }
            else
            {
                string ip;
                uint16_t port;
                GetIPPort(candidateSock, ip, port);
                string ErrorContent = ip + ":" + to_string(port) + "event: " + to_string(candidateSockEvents);
                l.writeLog(Log::ERROR, ErrorContent);

                l.writeLog(Log::WARNING, "Shouldn't appear2");
            }
        }
    }

    delete[] packet;
}

void control::EpollAdd(const int &epfd, const int &fd, const uint32_t &events)
{
    string InfoContent = "EpollAdd ";
    l.writeLog(Log::INFO, InfoContent);
    static epoll_event ep_ev;
    ep_ev.data.fd = fd;
    ep_ev.events = events;
    if (-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, ep_ev.data.fd, &ep_ev))
    {
        perror("epoll_ctl()EPOLLADD");
        l.writeLog(Log::ERROR, string("epoll_ctl()") + strerror(errno));
    }
}

int control::EpollWait(const int &epfd, epoll_event *ep_events)
{
    // l.writeLog(Log::INFO, "epollwait begin");
    int cnt;
    cnt = epoll_wait(epfd, ep_events, this->maxEvent, -1);
    if (-1 == cnt)
    {
        perror("epoll_wait()");
        l.writeLog(Log::ERROR, string("epoll_wait()") + strerror(errno));
    }
    // l.writeLog(Log::INFO, string("epollwait end, cnt: ") + to_string(cnt));
    return cnt;
}

void control::EpollDel(const int &epfd, const int &fd)
{
    l.writeLog(Log::INFO, "Epolldel");
    if (-1 == epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr))
    {
        perror("epoll_ctl()EpollDel");
        l.writeLog(Log::ERROR, string("epoll_ctl()") + strerror(errno));
    }
}

bool control::GetIPPort(const uint32_t &sock, string &ip, uint16_t &port)
{
    size_t temp;
    if (getpeername(sock, (sockaddr *)&addr, (socklen_t *)&temp) == -1)
    {
        l.writeLog(Log::ERROR, "Get ip and port failed");

        return false;
    }

    ip = string(inet_ntoa(addr.sin_addr));
    port = addr.sin_port;

    return true;
}