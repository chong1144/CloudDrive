#include "CTRL.h"

CTRL::CTRL()
{}

CTRL::CTRL(const string& configFile)
{
    init(configFile);
    epfd = epoll_create1(0);
    if(epfd==-1){
        fileLog.writeLog(Log::ERROR, string("epoll_create1 error")+strerror(errno));
        exit(-1);
    }
    startServer();
    EpollAdd(this->fifo_dtoc, EPOLLIN);
    EpollAdd(this->socklisten, EPOLLIN);

    run();
}

void CTRL::EpollAdd (int fd, uint32_t events)
{
    fileLog.writeLog (Log::INFO, "EpollAdd");
    static epoll_event ep_ev;
    ep_ev.data.fd = fd;
    ep_ev.events = events;
    if (-1 == epoll_ctl (epfd, EPOLL_CTL_ADD, ep_ev.data.fd, &ep_ev)) {
        perror ("epoll_ctl()");
        fileLog.writeLog (Log::ERROR, string ("epoll_ctl()") + strerror (errno));
    }
}

int CTRL::EpollWait ()
{
    fileLog.writeLog (Log::INFO, "epollwait begin");
    int cnt;
    cnt = epoll_wait (epfd, this->ep_events, MAXEVENTS, -1);
    if (-1 == cnt) {
        perror ("epoll_wait()");
        fileLog.writeLog (Log::ERROR, string ("epoll_wait()") + strerror (errno));
    }
    fileLog.writeLog (Log::INFO, string ("epollwait end, len: ") + to_string (cnt));
    return cnt;
}

void CTRL::EpollDel (int fd)
{
    fileLog.writeLog (Log::INFO, "Epolldel");
    if (-1 == epoll_ctl (epfd, EPOLL_CTL_DEL, fd, nullptr)) {
        perror ("epoll_ctl()");
        fileLog.writeLog (Log::ERROR, string ("epoll_ctl()") + strerror (errno));
    }
}



bool CTRL::createPipe(const string &pipe)
{
    umask(0);
    if (mkfifo(pipe.c_str(), S_IFIFO | 0666) == -1)
    {
        string ErrorContent = "Fail to create pipe " + pipe;
        fileLog.writeLog(Log::WARNING, ErrorContent);
        return false;
    }

    string InfoContent = "create pipe " + pipe + " successfully!";
    fileLog.writeLog(Log::INFO, InfoContent);
    return true;
}

void CTRL::init(const string& configFile)
{
    Config c(configFile);

    port = atoi(c.getValue("Control", "port").c_str());
    ip = c.getValue("Control", "ip");
    numListen = atoi(c.getValue("Control", "numListen").c_str());
    maxEvent = atoi(c.getValue("Control", "maxEvent").c_str()) + 2;

    // 初始化日志需要用的参数
    fileLog.init(c.getValue("Control", "controlLogPosition"));

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

    fileLog.writeLog(Log::INFO, "create pipes successfully!");

    // 打开管道 没有检查是否打开成功 关闭一些目前不需要的通道
    this->fifo_ctod = open(c.getValue("Global", "path_fifo_ctod").c_str(), O_WRONLY);
    this->fifo_dtoc = open(c.getValue("Global", "path_fifo_dtoc").c_str(), O_RDONLY);
    fileLog.writeLog(Log::INFO, "open ipc with database successfully!");

    fileLog.writeLog(Log::INFO, "control read config file successfully!");

}

void CTRL::openfifo()
{

}

void CTRL::startServer()
{
    // 建立起连接 采用非阻塞的方式进行一切的操作
    int ret = 0;

    // 创建socket 设置非阻塞
    socklisten = socket(AF_INET, SOCK_STREAM, 0);
    if (socklisten == -1)
    {
        fileLog.writeLog(Log::ERROR, "Fail to create server socket");
        exit(-1);
    }
    else if(0)
    {
        int flag = fcntl(socklisten, F_GETFL);
        fcntl(socklisten, flag | O_NONBLOCK);
    }

    // 设置地址复用等
    int opt = 1;
    ret = setsockopt(socklisten, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    if (ret == -1)
    {
        fileLog.writeLog(Log::ERROR, "Fail to setsockopt");
        exit(-1);
    }

    // 设置地址等连接参数
    if (inet_pton(AF_INET, this->ip.c_str(), &addr.sin_addr) <= 0)
    {
        fileLog.writeLog(Log::ERROR, "invalid address");
        exit(-1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(this->port);

    // 绑定地址
    ret = bind(socklisten, (sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
    {
        fileLog.writeLog(Log::ERROR, "bind error");
        exit(-1);
    }

    ret = listen(socklisten, numListen);
    if (ret < 0)
    {
        fileLog.writeLog(Log::ERROR, "listen error");
        exit(-1);
    }

    string InfoContent = "Start Server at " + ip + ":" + to_string(port);
    InfoContent += " with mutiple " + to_string(numListen) + " listeners";
    fileLog.writeLog(Log::INFO, InfoContent);
}

int CTRL::handleNewConnection(socket_t sockclnt)
{
    allSet.insert(sockclnt);
    this->EpollAdd(sockclnt, EPOLLIN);
    return 0;
}

int CTRL::handleSigninResFromDB()
{
    int len;
    // read signinres
    len = read(this->fifo_dtoc, &signinresPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("handleSigninResFromDB read head len:")+to_string(len));
        exit(-1);
    }
    // 成功登录
    if(signinresPacket.code == SIGNIN_SUCCESS){
        session_t session(signinresPacket.Session);
        // 建立映射
        soseMap[queryQue.front()] = session;
        sesoMap[session] = queryQue.front(); 
    }else{
        fileLog.writeLog(Log::WARNING, "Signin failed");
    }
    // 回包
    this->sendSigninResToClient(queryQue.front());
    // 删除
    queryQue.pop();
}

int CTRL::handleSignupResFromDB()
{
    int len;
    // read signinres
    len = read(this->fifo_dtoc, &signupresPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("handleSigninResFromDB read head len:")+to_string(len));
        exit(-1);
    }
    // 成功注册
    if(signupresPacket.code == SIGNUP_SUCCESS){
        
    }else{
        fileLog.writeLog(Log::WARNING, "Signin failed");
    }
    // 回包
    this->sendSignupResToClient(queryQue.front());
    // 删除
    queryQue.pop();
}

int CTRL::handleCopyRespFromDB()
{
    int len;
    len = read(this->fifo_dtoc, &copyRespPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("handleCopyRespFromDB read head len:")+to_string(len));
        exit(-1);
    }
    // 回包
    this->sendCopyRespToCilent(queryQue.front());
    // 删除
    queryQue.pop();
}

int CTRL::handleDeleteRespFromDB()
{
    int len;
    // read signinres
    len = read(this->fifo_dtoc, &deleteRespPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("handleDeleteRespFromDB read deleteRespPacket len:")+to_string(len));
        exit(-1);
    }
    // 回包
    this->sendDeleteRespToCilent(queryQue.front());
    // 删除
    queryQue.pop();
}

int CTRL::handleMkdirRespFromDB()
{
    int len;
    // read signinres
    len = read(this->fifo_dtoc, &mkdirRespPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("handleMkdirRespFromDB read mkdirRespPacket len:")+to_string(len));
        exit(-1);
    }
    // 回包
    this->sendMkdirRespToCilent(queryQue.front());
    // 删除
    queryQue.pop();
}

int CTRL::handleMoveRespFromDB()
{
    int len;
    // read signinres
    len = read(this->fifo_dtoc, &moveRespPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("handleMoveRespFromDB read moveRespPacket len:")+to_string(len));
        exit(-1);
    }
    // 回包
    this->sendMoveRespToCilent(queryQue.front());
    // 删除
    queryQue.pop();
}

int CTRL::handleSynRespFromDB()
{
    int len;
    // read signinres
    len = read(this->fifo_dtoc, &synRespPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("handleSynRespFromDB read synRespPacket len:")+to_string(len));
        exit(-1);
    }
    // 回包
    this->sendSynRespToCilent(queryQue.front());
    // 删除
    queryQue.pop();
}

int CTRL::handlePacketFromDB()
{
    fileLog.writeLog(Log::INFO, string("handlePacketFromDB begin"));
    int len;
    len = read (fifo_dtoc, &headPacket, headlen);
    if (len != headlen) {
        fileLog.writeLog (Log::ERROR, string ("handlePacketFromDB 读取header from DB, 长度错误 len: ") + to_string (len));
        return -1;
    }
    if (headPacket.p == pType::SIGNIN_RES){
        handleSigninResFromDB();
    }else if(headPacket.p == pType::SIGNUP_RES){
        handleSignupResFromDB();
    }else if(headPacket.p == pType::COPY_RES){
        handleCopyRespFromDB();
    }else if(headPacket.p == pType::DELETE_RES){
        handleDeleteRespFromDB();
    }else if(headPacket.p == pType::MKDIR){
        handleMkdirRespFromDB();
    }else if(headPacket.p == pType::MOVE_RES){
        handleMoveRespFromDB();
    }else if(headPacket.p == pType::SYN_RESP){
        handleSynRespFromDB();
    }

    fileLog.writeLog(Log::INFO, string("handlePacketFromDB end"));
    return 0;
}

int CTRL::handlePacketFromClient(socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("handlePacketFromClient begin"));
    int len;
    len = read(sockclnt, &headPacket, headlen);
    if(len == 0){
        handleDisconnection(sockclnt);
        return 0;
    }
    if(len != headlen){
        fileLog.writeLog (Log::ERROR, string ("handlePacketFromClient 读取header from Client, 长度错误 len: ") + to_string (len));
        return -1;
    }
    if(headPacket.p==pType::SIGNIN){
        handleSigninFromClient(sockclnt);
    }else if(headPacket.p==pType::SIGNUP){
        handleSignupFromClient(sockclnt);
    }else if(headPacket.p==pType::COPY){
        handleCopyFromClient(sockclnt);
    }else if(headPacket.p==pType::DELETE){
        handleDeleteFromClient(sockclnt);
    }else if(headPacket.p==pType::MKDIR){
        handleMkdirFromClient(sockclnt);
    }else if(headPacket.p==pType::MOVE){
        handleMoveFromClient(sockclnt);
    }else if(headPacket.p==pType::SYN_REQ){
        handleSynReqFromClient(sockclnt);
    }
    fileLog.writeLog (Log::INFO, string ("handlePacketFromClient end"));
    return 0;
}

int CTRL::handleSigninFromClient(socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("handleSigninFromClient begin"));
    int len;
    len = read(sockclnt, &signinPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog (Log::ERROR, string ("handleSigninFromClient 读取signin from Client, 长度错误 len: ") + to_string (len));
        return -1;
    }
    queryQue.push(sockclnt);

    this->sendSigninToDB();
    fileLog.writeLog (Log::INFO, string ("handleSigninFromClient end"));
    return 0;
}

int CTRL::handleSignupFromClient(socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("handleSignupFromClient begin"));
    int len = read(sockclnt, &signupPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog (Log::ERROR, string ("handleSignupFromClient 读取signin from Client, 长度错误 len: ") + to_string (len));
        return -1;
    }
    queryQue.push(sockclnt);

    this->sendSignupToDB();
    fileLog.writeLog (Log::INFO, string ("handleSignupFromClient end"));
    return 0;
}

int CTRL::handleCopyFromClient(socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("handleCopyFromClient begin"));
    int len = read(sockclnt, &copyPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog (Log::ERROR, string ("handleCopyFromClient 读取copy from Client, 长度错误 len: ") + to_string (len));
        return -1;
    }
    queryQue.push(sockclnt);
    this->sendCopyToDB();
    fileLog.writeLog (Log::INFO, string ("handleCopyFromClient end"));
    return 0;
}

int CTRL::handleDeleteFromClient(socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("handleDeleteFromClient begin"));
    int len = read(sockclnt, &deletePacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog (Log::ERROR, string ("handleDeleteFromClient 读取delete from Client, 长度错误 len: ") + to_string (len));
        return -1;
    }
    queryQue.push(sockclnt);

    this->sendDeleteToDB();
    fileLog.writeLog (Log::INFO, string ("handleDeleteFromClient end"));
    return 0;
}

int CTRL::handleMoveFromClient(socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("handleMoveFromClient begin"));
    int len = read(sockclnt, &movePacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog (Log::ERROR, string ("handleMoveFromClient 读取Move from Client, 长度错误 len: ") + to_string (len));
        return -1;
    }
    queryQue.push(sockclnt);

    this->sendMoveToDB();
    fileLog.writeLog (Log::INFO, string ("handleMoveFromClient end"));
    return 0;
}

int CTRL::handleMkdirFromClient(socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("handleMkdirFromClient begin"));
    int len = read(sockclnt, &mkdirPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog (Log::ERROR, string ("handleMkdirFromClient 读取Mkdir from Client, 长度错误 len: ") + to_string (len));
        return -1;
    }
    queryQue.push(sockclnt);

    this->sendMkdirToDB();
    fileLog.writeLog (Log::INFO, string ("handleMkdirFromClient end"));
    return 0;
}

int CTRL::handleSynReqFromClient(socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("handleSynReqFromClient begin"));
    int len = read(sockclnt, &synReqPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog (Log::ERROR, string ("handleSynReqFromClient 读取SynReq from Client, 长度错误 len: ") + to_string (len));
        return -1;
    }
    queryQue.push(sockclnt);

    this->sendSynReqToDB();
    fileLog.writeLog (Log::INFO, string ("handleSynReqFromClient end"));
    return 0;
}


int CTRL::sendSigninResToClient(socket_t sockclnt)
{
    int len;
    headPacket.p = pType::SIGNIN_RES;
    headPacket.len = PackageSizeMap.at(pType::SIGNIN_RES);
    len = write(sockclnt, &headPacket, headlen);
    if(len!=headlen){
        fileLog.writeLog(Log::ERROR, string("sendSigninResToClient write head len:")+to_string(len));
        return -1;
    }
    len = write(sockclnt, &signinresPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendSigninResToClient write signinres len:")+to_string(len));
        return -1;
    }
    return 0;
}

int CTRL::sendSignupResToClient(socket_t sockclnt)
{
    int len;
    headPacket.p = pType::SIGNUP_RES;
    headPacket.len = PackageSizeMap.at(pType::SIGNUP_RES);
    len = write(sockclnt, &headPacket, headlen);
    if(len!=headlen){
        fileLog.writeLog(Log::ERROR, string("sendSignupResToClient write head len:")+to_string(len));
        return -1;
    }
    len = write(sockclnt, &signupresPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendSignupResToClient write signupres len:")+to_string(len));
        return -1;
    }
    return 0;
}

int CTRL::sendCopyRespToCilent(socket_t sockclnt)
{
    int len;
    headPacket.p = pType::COPY_RES;
    headPacket.len = PackageSizeMap.at(pType::COPY_RES);
    len = write(sockclnt, &headPacket, headlen);
    if(len!=headlen){
        fileLog.writeLog(Log::ERROR, string("sendCopyRespToCilent write head len:")+to_string(len));
        return -1;
    }
    len = write(sockclnt, &copyRespPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendCopyRespToCilent write copyRespPacket len:")+to_string(len));
        return -1;
    }
    return 0;
}

int CTRL::sendDeleteRespToCilent(socket_t sockclnt)
{
    int len;
    headPacket.p = pType::DELETE_RES;
    headPacket.len = PackageSizeMap.at(pType::DELETE_RES);
    len = write(sockclnt, &headPacket, headlen);
    if(len!=headlen){
        fileLog.writeLog(Log::ERROR, string("sendDeleteRespToCilent write head len:")+to_string(len));
        return -1;
    }
    len = write(sockclnt, &deleteRespPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendDeleteRespToCilent write deleteRespPacket len:")+to_string(len));
        return -1;
    }
    return 0;
}

int CTRL::sendMkdirRespToCilent(socket_t sockclnt)
{
    int len;
    headPacket.p = pType::MKDIR_RES;
    headPacket.len = PackageSizeMap.at(pType::MKDIR_RES);
    len = write(sockclnt, &headPacket, headlen);
    if(len!=headlen){
        fileLog.writeLog(Log::ERROR, string("sendMkdirRespToCilent write head len:")+to_string(len));
        return -1;
    }
    len = write(sockclnt, &mkdirRespPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendMkdirRespToCilent write mkdirRespPacket len:")+to_string(len));
        return -1;
    }
    return 0;
}

int CTRL::sendMoveRespToCilent(socket_t sockclnt)
{
    int len;
    headPacket.p = pType::MOVE_RES;
    headPacket.len = PackageSizeMap.at(pType::MOVE_RES);
    len = write(sockclnt, &headPacket, headlen);
    if(len!=headlen){
        fileLog.writeLog(Log::ERROR, string("sendMoveRespToCilent write head len:")+to_string(len));
        return -1;
    }
    len = write(sockclnt, &moveRespPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendMoveRespToCilent write moveRespPacket len:")+to_string(len));
        return -1;
    }
    return 0;
}

int CTRL::sendSynRespToCilent(socket_t sockclnt)
{
    int len;
    headPacket.p = pType::SYN_RESP;
    headPacket.len = PackageSizeMap.at(pType::SYN_RESP);
    len = write(sockclnt, &headPacket, headlen);
    if(len!=headlen){
        fileLog.writeLog(Log::ERROR, string("sendSynRespToCilent write head len:")+to_string(len));
        return -1;
    }
    len = write(sockclnt, &synRespPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendSynRespToCilent write synRespPacket len:")+to_string(len));
        return -1;
    }
    return 0;
}

int CTRL::sendSignupToDB()
{
    fileLog.writeLog(Log::INFO, string("sendSignupToDB begin"));
    int len;
    len = write(this->fifo_ctod, &headPacket, headlen);
    if(len != headlen){
        fileLog.writeLog(Log::ERROR, string("sendSignupToDB write head len:")+to_string(len));
        return -1;
    }
    len = write(this->fifo_ctod, &signupPacket, headPacket.len);
    if(len != headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendSignupToDB write signup len:")+to_string(len));
        return -1;
    }
    fileLog.writeLog(Log::INFO, string("sendSignupToDB end"));
}

int CTRL::sendSigninToDB()
{
    fileLog.writeLog(Log::INFO, string("sendSigninToDB begin"));
    int len;
    len = write(this->fifo_ctod, &headPacket, headlen);
    if(len != headlen){
        fileLog.writeLog(Log::ERROR, string("sendSigninToDB write head len:")+to_string(len));
        return -1;
    }
    len = write(this->fifo_ctod, &signinPacket, headPacket.len);
    if(len != headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendSigninToDB write signin len:")+to_string(len));
        return -1;
    }
    fileLog.writeLog(Log::INFO, string("sendSigninToDB end"));
}

int CTRL::sendCopyToDB()
{
    fileLog.writeLog(Log::INFO, string("sendCopyToDB begin"));
    int len;
    len = write(this->fifo_ctod, &headPacket, headlen);
    if(len != headlen){
        fileLog.writeLog(Log::ERROR, string("sendCopyToDB write head len:")+to_string(len));
        return -1;
    }
    len = write(this->fifo_ctod, &copyPacket, headPacket.len);
    if(len != headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendCopyToDB write copy len:")+to_string(len));
        return -1;
    }
    fileLog.writeLog(Log::INFO, string("sendCopyToDB end"));
}

int CTRL::sendDeleteToDB()
{
    fileLog.writeLog(Log::INFO, string("sendDeleteToDB begin"));
    int len;
    len = write(this->fifo_ctod, &headPacket, headlen);
    if(len != headlen){
        fileLog.writeLog(Log::ERROR, string("sendDeleteToDB write head len:")+to_string(len));
        return -1;
    }
    len = write(this->fifo_ctod, &deletePacket, headPacket.len);
    if(len != headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendDeleteToDB write delete len:")+to_string(len));
        return -1;
    }
    fileLog.writeLog(Log::INFO, string("sendDeleteToDB end"));
}

int CTRL::sendMkdirToDB()
{
    fileLog.writeLog(Log::INFO, string("sendMkdirToDB begin"));
    int len;
    len = write(this->fifo_ctod, &headPacket, headlen);
    if(len != headlen){
        fileLog.writeLog(Log::ERROR, string("sendMkdirToDB write head len:")+to_string(len));
        return -1;
    }
    len = write(this->fifo_ctod, &mkdirPacket, headPacket.len);
    if(len != headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendMkdirToDB write mkdir len:")+to_string(len));
        return -1;
    }
    fileLog.writeLog(Log::INFO, string("sendMkdirToDB end"));
}

int CTRL::sendMoveToDB()
{
    fileLog.writeLog(Log::INFO, string("sendMoveToDB begin"));
    int len;
    len = write(this->fifo_ctod, &headPacket, headlen);
    if(len != headlen){
        fileLog.writeLog(Log::ERROR, string("sendMoveToDB write head len:")+to_string(len));
        return -1;
    }
    len = write(this->fifo_ctod, &movePacket, headPacket.len);
    if(len != headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendMoveToDB write move len:")+to_string(len));
        return -1;
    }
    fileLog.writeLog(Log::INFO, string("sendMoveToDB end"));
}

int CTRL::sendSynReqToDB()
{
    fileLog.writeLog(Log::INFO, string("sendSynReqToDB begin"));
    int len;
    len = write(this->fifo_ctod, &headPacket, headlen);
    if(len != headlen){
        fileLog.writeLog(Log::ERROR, string("sendSynReqToDB write head len:")+to_string(len));
        return -1;
    }
    len = write(this->fifo_ctod, &synReqPacket, headPacket.len);
    if(len != headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendSynReqToDB write synReq len:")+to_string(len));
        return -1;
    }
    fileLog.writeLog(Log::INFO, string("sendSynReqToDB end"));
}


bool CTRL::GetIPPort(const uint32_t &sock, string &ip, uint16_t &port)
{
    size_t temp;
    if (getpeername(sock, (sockaddr *)&addr, (socklen_t *)&temp) == -1)
    {
        fileLog.writeLog(Log::ERROR, "Get ip and port failed");

        return false;
    }

    ip = string(inet_ntoa(addr.sin_addr));
    port = addr.sin_port;

    return true;
}

void CTRL::handleDisconnection(socket_t sockclnt)
{
    EpollDel(sockclnt);
    if(soseMap.count(sockclnt)){
        sesoMap.erase(soseMap[sockclnt]);
        soseMap.erase(sockclnt);
    }
    close(sockclnt);
}

void CTRL::run()
{
    int epcnt;
    while(true)
    {
        epcnt = EpollWait();
        for(int i=0; i<epcnt;++i){
            auto& ep_ev = this->ep_events[i];
            // 数据库
            if(ep_ev.data.fd == fifo_dtoc){
                if(ep_ev.events & EPOLLERR){
                    fileLog.writeLog(Log::ERROR, string("EPOLLERR socket fifo_dtoc, exit!"));
                    exit(-1);
                }
                else if(ep_ev.events & EPOLLIN){
                    handlePacketFromDB ();
                }
            }
            // 新连接
            else if(ep_ev.data.fd == this->socklisten){
                if(ep_ev.events & EPOLLERR){
                    fileLog.writeLog(Log::ERROR, string("EPOLLERR socket listensocket"));
                    EpollDel(ep_ev.data.fd);
                    exit(-1);
                }
                else if(ep_ev.events & EPOLLIN){
                    int sockclnt = accept(socklisten, nullptr, nullptr);
                    if(sockclnt==-1){
                        fileLog.writeLog(Log::ERROR, string("accept error"));
                        exit(-1);
                    }


                    handleNewConnection(sockclnt);
                }
                // fileLog.writeLog(Log::INFO, string(""));
            }
            // 已经创建的连接
            for(auto& sock:allSet){
                if(ep_ev.data.fd==sock){
                    if(ep_ev.events&EPOLLERR){
                        fileLog.writeLog(Log::ERROR, string("EPOLLERR socket")+ to_string(sock));
                        exit(-1);
                    }
                    if(ep_ev.events&EPOLLIN){
                        this->handlePacketFromClient(sock);
                    }
                }   
            }

        }
    }
}