#include "Downloader.h"

Downloader::Downloader(const string& config_file) :config(config_file)
{
    this->init(config_file);
    this->filein.init(config_file);
    epfd = epoll_create1(0);
    if(epfd==-1){
        fileLog.writeLog(Log::ERROR, string("epoll_create1 error")+strerror(errno));
        exit(-1);
    }
    // open fifo
    this->openfifo();

    this->startServer();
}

void Downloader::openfifo()
{
    //open fifo
    fifo_db_w = open (config.getValue ("Global", "path_fifo_stod").c_str (), O_WRONLY);
    fifo_db_r = open (config.getValue ("Global", "path_fifo_dtos").c_str (), O_RDONLY);
    fifo_ctrl_w = open (config.getValue ("Global", "path_fifo_stoc").c_str (), O_WRONLY);
    fifo_ctrl_r = open (config.getValue ("Global", "path_fifo_ctos").c_str (), O_RDONLY);
    if(fifo_db_w==-1||fifo_db_r==-1||fifo_ctrl_r==-1||fifo_ctrl_w==-1){
        fileLog.writeLog(Log::ERROR, string("open fifo error, exit!!!"));
        exit(-1);
    }
}

void Downloader::init(const string& config_file)
{
    Config c(config_file);
    this->port = atoi(c.getValue("Downloader", "port").c_str());
    this->ip = c.getValue("Downloader", "ip");
    this->numListen = atoi(c.getValue("Downloader", "numListen").c_str());
    this->maxEvent = atoi(c.getValue("Downloader", "maxEvent").c_str())+2;
    fileLog.init(c.getValue("Downloader","LogPosition"));
}

void Downloader::startServer()
{
    int ret;

    this->socklisten = socket(AF_INET, SOCK_STREAM, 0);
    if(socklisten==-1){
        fileLog.writeLog(Log::ERROR, string("Fail to create listen socket"));
        exit(-1);
    }
    
    // 设置地址复用
    int opt = 1;
    ret = setsockopt(socklisten, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    if(ret==-1){
        fileLog.writeLog(Log::ERROR, string("Failed to setsockopt"));
        exit(-1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(this->port);

    // 绑定地址
    ret = bind(this->socklisten, (sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
    {
        fileLog.writeLog(Log::ERROR, "bind error");
        exit(-1);
    }

    ret = listen(this->socklisten, numListen);
    if (ret < 0)
    {
        fileLog.writeLog(Log::ERROR, "listen error");
        exit(-1);
    }

    string InfoContent = "Start Server at " + ip + ":" + to_string(port);
    InfoContent += " with mutiple " + to_string(numListen) + " listeners";
    fileLog.writeLog(Log::INFO, InfoContent);
}

void Downloader::EpollAdd (int fd, uint32_t events)
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

int Downloader::EpollWait ()
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

void Downloader::EpollDel (int fd)
{
    fileLog.writeLog (Log::INFO, "Epolldel");
    if (-1 == epoll_ctl (epfd, EPOLL_CTL_DEL, fd, nullptr)) {
        perror ("epoll_ctl()");
        fileLog.writeLog (Log::ERROR, string ("epoll_ctl()") + strerror (errno));
    }
}

int Downloader::run()
{
    int epcnt;
    while(true)
    {
        epcnt = EpollWait();
        for(int i=0;i<epcnt;++i){
            auto &ep_ev = this->ep_events[i];
            // from ctrl
            if(ep_ev.data.fd == fifo_ctrl_r){

            }
            else if(ep_ev.data.fd==fifo_db_r){
                if(ep_ev.events & EPOLLERR){
                    fileLog.writeLog(Log::ERROR, string("EPOLLERR fifo_db_r EpollDel!"));
                }
                if(ep_ev.events & EPOLLIN){
                    this->handlePacketFromDB ();
                }
            }
            // 建立新的连接
            else if(ep_ev.data.fd == this->socklisten){
                if(ep_ev.events & EPOLLERR){
                    fileLog.writeLog(Log::ERROR, string("EPOLLERR socket:")+to_string(socklisten));
                    EpollDel(ep_ev.data.fd);
                    exit(-1);
                }
                if(ep_ev.events & EPOLLIN){
                    int sockclnt = accept(socklisten, nullptr, nullptr);
                    if(sockclnt == -1){
                        fileLog.writeLog(Log::ERROR, string("accept error"));
                        exit(-1);
                    }
                    this->handleNewConnection(sockclnt);
                }
            }
            // from client
            for (auto& sock:allSet){
                if(sock == ep_ev.data.fd){
                    if(ep_ev.events & EPOLLERR){
                        fileLog.writeLog(Log::ERROR, string("EPOLLERR socket:")+to_string(sock));
                    }
                    if(ep_ev.events & EPOLLIN){
                        handlePacketFromClient(sock);
                    }
                }
            }
            fileLog.writeLog(Log::ERROR, string("there is a fd that was not handled"));
        }
    }
}

int Downloader::handleNewConnection(socket_t sockclnt)
{
    idleSet.insert(sockclnt);
    allSet.insert(sockclnt);
    EpollAdd(sockclnt, EPOLLIN);
    return 0;
}

int Downloader::handlePacketFromDB()
{
    fileLog.writeLog (Log::INFO, string ("handlePacketFromDB begin"));
    int len;
    // read head from db
    len = read (fifo_db_r, &headPacket, headlen);
    if (len != headlen) {
        fileLog.writeLog (Log::ERROR, string ("handlePacketFromDB 读取header from DB, 长度错误 len: ") + to_string (len));
        return -1;
    }    
    // read fileinfo from db
    if(headPacket.p!=pType::FILEINFO){
        fileLog.writeLog(Log::ERROR, string("handlePacketFromDB 收到的包不是FILEINFO"));
        return -1;
    }
    len = read (fifo_db_r, &fileInfoPacket, sizeof (fileInfoPacket));
    if (len != sizeof (fileInfoPacket)) {
        fileLog.writeLog (Log::ERROR, string ("handlePacketFromDB 读取fileinfo from DB, 长度错误 len: ") + to_string (len));
        return -1;
    }
    FileHash filehash(fileInfoPacket.md5, MD5Length);
    if(fileInfoPacket.exist&fileInfoPacket.completed){
        idleSet.insert(queryQue.front());
        sockFileMap[queryQue.front()] = filehash;
        queryQue.pop();
    }else{
        fileLog.writeLog(Log::ERROR, string("Target file is can't be downloaded, exist=")+to_string(fileInfoPacket.exist)
            +", completed="+to_string(fileInfoPacket.completed));
        return -1;
    }
    

    fileLog.writeLog (Log::INFO, string ("handlePacketFromDB end"));
    return 0;
}


int Downloader::handlePacketFromCTRL()
{
    return 0;
}
int Downloader::handlePacketFromClient(socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("handlePacketFromClient begin"));
    int len;
    if ((len = read (sockclnt, &headPacket, headlen)) != headlen) {
        fileLog.writeLog (Log::ERROR, string ("handlePacketFromClient 读取header from client长度错误 len: ") + to_string (len));
        return -1;
    }
    if(headPacket.p==pType::DOWNLOAD_REQ){
        if(idleSet.count(sockclnt)){
            fileLog.writeLog(Log::WARNING, string("socket is not in idleSet!!!"));
        }
        handleDownloadReq(sockclnt);
    }else if(headPacket.p==pType::DOWNLOAD_FETCH){
        handleFetchReq(sockclnt);
    }else if(headPacket.p==pType::DOWNLOAD_DONE){
        handleDoneReq(sockclnt);
    }
    // 剩余暂停和恢复请求还没完成
    fileLog.writeLog(Log::INFO, string("handlePacketFromClient end"));
    return 0;
}

int Downloader::handleDownloadReq(socket_t sockclnt)
{
    fileLog.writeLog(Log::INFO, string("handleDownloadReq begin"));
    int len;
    len = read(sockclnt, &downloadreqPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("handleDownloadReq 读取fetchBody from client 长度错误 len: ")+to_string(len));
        return -1;
    }
    this->sendReqFileHashToDB();
    queryQue.push(sockclnt);
    // 文件hash需要等到数据库返回结果才知道
    fileLog.writeLog(Log::INFO, string("handleDownloadReq end"));
}

int Downloader::handleFetchReq(socket_t sockclnt)
{
    fileLog.writeLog(Log::INFO, string("handleFetchReq begin"));
    int len;
    len = read(sockclnt, &fetchPacket, headPacket.len);
    if(len!=headPacket.len){
        fileLog.writeLog(Log::ERROR, string("handleFetchReq 读取fetchBody from client 长度错误 len: ")+to_string(len));
        return -1;
    }
    FileHash filehash = sockFileMap[sockclnt];
    if(string(fetchPacket.MD5, MD5Length)!=filehash){
        fileLog.writeLog(Log::WARNING, string("handleFetchReq 获取的文件块和列表中文件hash不符"));
        return -1;
    }
    FileChunk chunk;
    chunk.chunkNo = fetchPacket.chunkNo;
    memcpy(chunk.md5, fetchPacket.MD5, 64);
    filein.ReadFile(chunk); //读1M
    uint32_t i=0;
    uint32_t size = ChunkSize;
    memcpy(pushPacket.md5, chunk.md5, MD5Length);
    pushPacket.len = ContentSize;
    pushPacket.last = false;
    headPacket.p = pType::DOWNLOAD_PUSH;
    headPacket.len = PackageSizeMap.at(headPacket.p);
    while(true)
    {
        // 最后一个包可能的size[1,1024]
        if((size-=ContentSize)>ContentSize){
            memcpy(pushPacket.content, chunk.content + ContentSize*i, ContentSize);
            pushPacket.id = i++;
            len = write(sockclnt, &headPacket, headlen);
            if(len!=headlen){
                fileLog.writeLog(Log::ERROR, string("handleFetchReq 发送headPacket len=")+to_string(len) + ", id="+to_string(i));
            }
            len = write(sockclnt, &pushPacket, headPacket.len);
            if(len!=headPacket.len){
                fileLog.writeLog(Log::ERROR, string("handleFetchReq 发送pushPacket len=")+to_string(len) + ", id="+to_string(i));
            }
        }else{
            break;
        }
    }
    memcpy(pushPacket.content, chunk.content + ContentSize*i, size);
    pushPacket.id = i;
    pushPacket.last = true;
    pushPacket.len = size;
    len = write(sockclnt, &headPacket, headlen);
    if (len != headlen){
        fileLog.writeLog(Log::ERROR, string("handleFetchReq 发送headPacket len=") + to_string(len) + ", id="+to_string(i));
    }
    len = write(sockclnt, &pushPacket, headPacket.len);
    if (len != headPacket.len){
        fileLog.writeLog(Log::ERROR, string("handleFetchReq 发送pushPacket len=") + to_string(len) + ", id="+to_string(i));
    }
    fileLog.writeLog(Log::INFO, string("handleFetchReq end"));
}

int Downloader::handleDoneReq(socket_t sockclnt)
{
    sockFileMap.erase(sockclnt);
    idleSet.insert(sockclnt);
    return 0;
}

int Downloader::sendReqFileHashToDB()
{
    int len;
    len = write (fifo_db_w, &headPacket, headlen);
    if (len != headlen) {
        fileLog.writeLog (Log::ERROR, string ("sendReqFileInfotoDB() write() ret val=") + to_string (len));
        return -1;
    }

    len = write (fifo_db_w, &downloadreqPacket, sizeof (downloadreqPacket));
    if (len != sizeof (downloadreqPacket)) {
        fileLog.writeLog (Log::ERROR, string ("sendReqFileInfotoDB() write() ret val=") + to_string (len));
        return -1;
    }

    return 0;

}

int Downloader::sendRespToClient(socket_t sockclnt)
{
    headPacket.p = pType::DOWNLOAD_RESP;
    headPacket.len = PackageSizeMap.at(pType::DOWNLOAD_RESP);
    if(this->fileInfoPacket.completed & this->fileInfoPacket.exist){
        this->respPacket.state = DOWNLOAD_FILE_EXIST;
    }else{
        this->respPacket.state = DOWNLOAD_FILE_NOTEXIST;
    }
    this->respPacket.chunkNum = this->fileInfoPacket.size;
    int len;
    len = write(sockclnt, &headPacket, headlen);
    if (len != headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendRespToClient 发送headPacket len="));
        return -1;
    }
    len = write(sockclnt, &respPacket, headPacket.len);
    if (len != headPacket.len){
        fileLog.writeLog(Log::ERROR, string("sendRespToClient 发送respPacket len="));
        return -1;
    }
    return 0;
}
// int Downloader::sendReqToDB()
// {
    
// }