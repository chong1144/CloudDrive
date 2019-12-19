#include "Uploader.h"

Uploader::Uploader (string config_file, string log_file) :config (config_file), fileLog (log_file), fileout (FileIOPath)
{
    epfd = epoll_create1 (0);
    if (epfd == -1) {
        fileLog.writeLog (Log::ERROR, string ("epoll_create1 error") + strerror (errno));
        exit (-1);
    }
    //open fifo
    fifo_db_w = open (config.getValue ("Global", "path_fifo_rtod").c_str (), O_WRONLY);
    fifo_db_r = open (config.getValue ("Global", "path_fifo_dtor").c_str (), O_RDONLY);
    fifo_ctrl_w = open (config.getValue ("Global", "path_fifo_rtoc").c_str (), O_WRONLY);
    fifo_ctrl_r = open (config.getValue ("Global", "path_fifo_ctor").c_str (), O_RDONLY);
}




void Uploader::init(const string& configFile)
{
    Config c(configFile);
    this->port = atoi(c.getValue("Uploader", "port").c_str());
    this->ip = c.getValue("Uploader", "ip");
    this->numListen = atoi(c.getValue("Uploader", "numListen").c_str());
    this->maxEvent = atoi(c.getValue("Uploader", "maxEvent").c_str()) + 2;
    fileLog.init(c.getValue("Uploader","LogPosition"));
}

void Uploader::startServer()
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
    ret = bind(sock, (sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
    {
        fileLog.writeLog(Log::ERROR, "bind error");
        exit(-1);
    }

    ret = listen(sock, numListen);
    if (ret < 0)
    {
        fileLog.writeLog(Log::ERROR, "listen error");
        exit(-1);
    }

    string InfoContent = "Start Server at " + ip + ":" + to_string(port);
    InfoContent += " with mutiple " + to_string(numListen) + " listeners";
    fileLog.writeLog(Log::INFO, InfoContent);
}



void Uploader::EpollAdd (int fd, uint32_t events)
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

int Uploader::EpollWait ()
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

void Uploader::EpollDel (int fd)
{
    fileLog.writeLog (Log::INFO, "Epolldel");
    if (-1 == epoll_ctl (epfd, EPOLL_CTL_DEL, fd, nullptr)) {
        perror ("epoll_ctl()");
        fileLog.writeLog (Log::ERROR, string ("epoll_ctl()") + strerror (errno));
    }
}

// 向DB模块发送获取文件信息的请求
// 此函数仅在收到来自CTRL的请求后调用，head以及uploadreqbody都未变，直接转发即可
int Uploader::sendReqFileInfotoDB ()
{
    int len;
    len = write (fifo_db_w, &headPacket, headlen);
    if (len != headlen) {
        fileLog.writeLog (Log::ERROR, string ("sendReqFileInfotoDB() write() ret val=") + to_string (len));
        return -1;
    }

    len = write (fifo_db_w, &uploadReqPacket, sizeof (uploadReqPacket));
    if (len != sizeof (uploadReqPacket)) {
        fileLog.writeLog (Log::ERROR, string ("sendReqFileInfotoDB() write() ret val=") + to_string (len));
        return -1;
    }

    return 0;
};
// 向DB模块发送保存文件信息的请求
int Uploader::sendReqSaveFileInfotoDB (const string& filehash)
{
    int len;
    auto& fileinfo = fileMap.at (filehash);
    strncpy(fileInfoPacket.md5, filehash.data(), MD5Length);
    fileInfoPacket.completed = isCompleted (filehash);
    // exist和size应该不影响数据库的行为
    fileInfoPacket.exist = true;
    fileInfoPacket.size = fileinfo.size;

    headPacket.p = FILEINFO;
    // 长度由FileInfoBody和不定长的bitmap组成
    headPacket.len = sizeof (FileInfoBody) + fileInfoPacket.size;
    len = write (fifo_db_w, &headPacket, headlen);
    if (len != headlen) {
        fileLog.writeLog (Log::ERROR, string ("sendReqSaveFileInfotoDB() write head ret val=") + to_string (len));
        return -1;
    }
    len = write (fifo_db_w, &fileInfoPacket, sizeof (FileInfoBody));
    if (len != sizeof (FileInfoBody)) {
        fileLog.writeLog (Log::ERROR, string ("sendReqSaveFileInfotoDB() write fileInfoPacket ret val=") + to_string (len));
        return -1;
    }
    len = write (fifo_db_w, fileinfo.chunkBitMap.data (), fileinfo.chunkBitMap.size ());
    if (len != fileinfo.chunkBitMap.size ()) {
        fileLog.writeLog (Log::ERROR, string ("sendReqSaveFileInfotoDB() write chunkBitMap ret val=") + to_string (len));
        return -1;
    }
    return 0;
}

void Uploader::addfsTable (FileHash hashcode, socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("addfsTable begin") + to_string (sockclnt));
    for (auto sock : fileMap[hashcode].sockSet) {
        // 已经在file-socket表中
        if (sock == sockclnt) {
            fileLog.writeLog (Log::WARNING, string ("socket already exists") + to_string (sockclnt));
            return;
        }
    }
    fileLog.writeLog (Log::INFO, string ("add a new file-socket to fsTable"));
    fileMap[hashcode].sockSet.insert (sockclnt);

    fileLog.writeLog (Log::INFO, string ("addfsTable end") + to_string (sockclnt));
}

// 判断上传完成
bool Uploader::isCompleted (const FileHash& filehash)
{
    auto& fMap = this->fileMap[filehash];
    for (int i = 0; i < fMap.size; ++i) {
        if (fMap.chunkBitMap.at (i) != CHUNK_EXIST) {
            return false;
        }
    }
    return true;
}

// 返回下一个空位块，如果空位块编号为size，表明没有空块，不一定完全上传完毕！！！！
uint16_t Uploader::getNextChunkNo (const FileHash& filehash)
{
    auto& fMap = this->fileMap[filehash];
    int i;
    for (i = 0; i < fMap.size; ++i) {
        if (fMap.chunkBitMap.at (i) == '0') {
            break;
        }
    }
    return i;
}

// // 处理从控制模块来的信息包
// int Uploader::handleReqfromCtrl ()
// {
//     fileLog.writeLog (Log::INFO, string ("handleReqfromCtrl begin"));
//     int len;
//     int sockclnt;
//     // 读取header
//     if ((len = read (fifo_ctrl_r, &headPacket, headlen)) != headlen) {
//         fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl 读取header from Ctrl长度错误 len: ") + to_string (len));
//         return -1;
//     }

//     // 是上传文件请求
//     if (headPacket.p == pType::UPLOAD_REQ) {
//         fileLog.writeLog (Log::INFO, string ("handleReqfromCtrl 处理 UPLOAD_REQ"));
//         if ((len = read (fifo_ctrl_r, &uploadReqPacket, headPacket.len)) != headPacket.len) {
//             fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl 读取reqBody from Ctrl长度错误 len: ") + to_string (len));
//             return -1;
//         }
//         // 将刚接收的上传请求转发给database模块
//         sendReqFileInfotoDB ();

//         // need a socket from ctrl!!!!!
//         if ((len = read (fifo_ctrl_r, &sockclnt, sizeof (int))) != 4) {
//             fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl 读取socket from Ctrl错误 len: ") + to_string (len));
//             return -1;
//         }
//         // 将socket放进等待查询集合
//         fileLog.writeLog (Log::INFO, string ("handleReqfromCtrl 等待查询集合里添加sock: " + to_string (sockclnt)));
//         queryQue.push (sockclnt);

//         //// 将文件和socket关联添加到fs表 需要等到查询数据库完成再做

//         return 0;
//     }
//     // 还需要添加对暂停指令的响应，取消是否需要？
//     else {
//         fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl 接收到来自ctrl的错误请求"));
//         return -1;
//     }
// }


// 
int Uploader::handleUploadReq(socket_t sockclnt)
{
    fileLog.writeLog(Log::INFO, string("handleUploadReq begin"));
    int len;
    if ((len = read(fifo_ctrl_r, &uploadReqPacket, headPacket.len)) != headPacket.len)
    {
        fileLog.writeLog(Log::ERROR, string("handleUploadReq 读取reqBody from Ctrl长度错误 len: ") + to_string(len));
        return -1;
    }
    // 将刚接收的上传请求转发给database模块
    sendReqFileInfotoDB();

    // 将socket放进等待查询集合
    fileLog.writeLog(Log::INFO, string("handleUploadReq 等待查询集合里添加sock: " + to_string(sockclnt)));
    queryQue.push(sockclnt);

    // 建立映射
    sockFileMap[sockclnt] = string(uploadReqPacket.MD5, MD5Length);
    //// 将文件和socket关联添加到fs表 需要等到查询数据库完成再做
    fileLog.writeLog(Log::INFO, string("handleUploadReq end"));
}


// 处理push信号
int Uploader::handlePush (const string& filehash, socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("handlePush begin"));
    int len;
    if ((len = read (sockclnt, &pushPacket, headPacket.len)) != headPacket.len) {
        fileLog.writeLog (Log::ERROR, string ("handlePush 读取pushBody from client长度错误 len: ") + to_string (len));
        return -1;
    }
    auto& chunk = fileChunk_map[make_pair (filehash, sockclnt)];
    memcpy (chunk.content + chunk.size, pushPacket.content, pushPacket.len);
    chunk.size += pushPacket.len;
    if (pushPacket.last) {
        // 切换到idle状态
        uploadSet.erase(sockclnt);
        idleSet.insert(sockclnt);

        fileout.WriteFile (chunk);
        // 修改位示图
        fileMap.at (filehash).chunkBitMap[chunk.chunkNo] = CHUNK_EXIST;
        uint16_t chunkNo = getNextChunkNo (filehash);
        // 已经没有需要上传的块，无需fetch
        if (chunkNo==fileMap.at(filehash).size) {
            // 判断文件是否完整
            if (isCompleted (filehash)) {
                // 完整则调用完成函数扫尾
                uploadDone (filehash);
            }
        }
        // 有需要上传的块
        else {
            sendFetchToClient(filehash, chunkNo, sockclnt);
        }
        
        // 写完释放文件块
        fileChunk_map.erase (make_pair (filehash, sockclnt));
        fileChunk_map[make_pair (filehash, sockclnt)] = FileChunk{};
    }
    fileLog.writeLog (Log::INFO, string ("handlePush end"));
    return 0;
}

// 处理从数据库来的信息包
int Uploader::handlePacketFromDB ()
{
    char buf[64 * 1024];
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
    }
    len = read (fifo_db_r, &fileInfoPacket, sizeof (fileInfoPacket));
    if (len != sizeof (fileInfoPacket)) {
        fileLog.writeLog (Log::ERROR, string ("handlePacketFromDB 读取fileinfo from DB, 长度错误 len: ") + to_string (len));
        return -1;
    }
    // 到此收到完整的fileInfo头
    fileLog.writeLog (Log::INFO, "handlePacketFromDB 读取fileInfo from DB done!");
    // read bitmap from db
    len = read (fifo_db_r, buf, headPacket.len - sizeof (fileInfoPacket));
    if (len != headPacket.len - sizeof (fileInfoPacket)) {
        fileLog.writeLog (Log::ERROR, string ("handlePacketFromDB 读取bitmap from DB, 长度错误 len: ") + to_string (len));
        return -1;
    }
    FileHash filehash (fileInfoPacket.md5, MD5Length);


    // 到此收到完整的bitmap
    fileLog.writeLog (Log::INFO, "handlePacketFromDB 读取bitmap from DB done!");
    fileLog.writeLog (Log::INFO, string ("filehash=") + filehash
        + ", exist=" + to_string (fileInfoPacket.exist)
        + ", completed=" + to_string (fileInfoPacket.completed)
        + ", size=" + to_string (fileInfoPacket.size));
    fileLog.writeLog (Log::INFO, string ("bitmap: ") + string (buf, fileInfoPacket.size));


    // 判断文件 存在&完整 or 存在&不完整
        // 文件存在&完整
    if (fileInfoPacket.completed && fileInfoPacket.exist) {
        // 加入完成队列，秒传
        fileLog.writeLog (Log::INFO, string ("handlePacketFromDB 秒传! socket: ") + to_string (queryQue.front ()));
        
        sendDoneToClient(filehash, queryQue.front (), true);
        // doneQue.push (queryQue.front ());
        // 从等待查询队列里删除
        queryQue.pop ();
    }
    // 存在&不完整
    else if (~fileInfoPacket.completed) {
        fileLog.writeLog (Log::INFO, string ("handlePacketFromDB 上传任务开始") + to_string (queryQue.front ()));

        // 放进idle集合，idle作为连接的初始状态
        idleSet.insert (queryQue.front ());
        // 建立一个新的映射md5-{bitmap, size, socketSet}
        FileLinker filelinker;
        filelinker.size = fileInfoPacket.size;
        filelinker.sockSet.insert (queryQue.front ());
        filelinker.chunkBitMap = move (string (buf, len));
        // insert pair, string 有copy constructor应该没问题

        fileMap.insert (make_pair (filehash, filelinker));
        fileLog.writeLog (Log::INFO, string ("handlePacketFromDB 添加md5-{bitmap, size, socketSet}映射") + to_string (queryQue.front ()));
        uint16_t chunkNo = getNextChunkNo (filehash);
        // 已经不完整，不用chunkNo==size判断完整性

        // 向client发送chunk请求，sock转为upload状态
        sendFetchToClient (filehash, chunkNo, queryQue.front ());
        queryQue.pop ();
    }
    else {
        fileLog.writeLog (Log::WARNING, string ("handlePacketFromDB fileInfo未处理的情况") + to_string (queryQue.front ()));
    }
    // 只要向数据库发出UPLOAD请求，不存在就被创建，一定存在，不必考虑不存在

    fileLog.writeLog (Log::INFO, string ("handlePacketFromDB end"));
    return 0;
}

// 接收并处理来自Client的packet
int Uploader::handlePacketFromClient(socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("handlePacketFromClient begin"));
    int len;
    if ((len = read (fifo_ctrl_r, &headPacket, headlen)) != headlen) {
        fileLog.writeLog (Log::ERROR, string ("handlePacketFromClient 读取header from client长度错误 len: ") + to_string (len));
        return -1;
    }
    if(headPacket.p==pType::UPLOAD_REQ){
        if(idleSet.count(sockclnt)){
            fileLog.writeLog(Log::WARNING, string("socket is not in idleSet!!!"));
            return -1;
        }
        handleUploadReq(sockclnt);
    }else if(headPacket.p==pType::UPLOAD_PUSH){
        handlePush(sockFileMap[sockclnt], sockclnt);
    }
    // 剩余暂停和恢复请求还没完成
    fileLog.writeLog(Log::INFO, string("handlePacketFromClient end"));
    return 0;
}

// 处理新的连接
// 设置状态为idle
// 添加EPOLL监视
int Uploader::handleNewConnect(socket_t sockclnt)
{
    // 新的连接，都是idle
    idleSet.insert(sockclnt);
    allSet.insert(sockclnt);
    // 添加监视
    EpollAdd(sockclnt, EPOLLIN);

    return 0;
}

// 向Client发送请求块号
// 修改socket状态为upload
// 建立(md5, socket)-->单个文件块映射
// 每轮查找有空闲socket时调用
int Uploader::sendFetchToClient (const string& filehash, uint16_t chunkNo, socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("sendReqToClient begin"));
    int len;
    memcpy (this->fetchPacket.MD5, filehash.data (), MD5Length);
    fetchPacket.chunkNo = chunkNo;
    // set head
    headPacket.p = UPLOAD_FETCH;
    headPacket.len = PackageSizeMap.at (UPLOAD_FETCH);
    if (headlen != (len = write (sockclnt, &headPacket, headlen))) {
        fileLog.writeLog (Log::ERROR, string ("sendReqToClient 发送head长度错误 len: ") + to_string (len));
        return -1;
    }

    if (headPacket.len != (len = write (sockclnt, &fetchPacket, headPacket.len))) {
        fileLog.writeLog (Log::ERROR, string ("sendReqToClient 发送fetch包长度错误 len: ") + to_string (len));
        return -1;
    }
    // 到此，向client发送请求块完成

    // 修改位示图
    fileMap[filehash].chunkBitMap[chunkNo] = CHUNK_UPLOADING;

    // 修改socket状态
    if (idleSet.count (sockclnt)) {
        idleSet.erase (sockclnt);
    }
    else {
        fileLog.writeLog (Log::WARNING, string ("sendReqToClient 目标socket先前不在idle集合中"));
    }
    // 改为正在传输状态
    uploadSet.insert (sockclnt);

    // 依据md5和socket创建文件块映射
    fileChunk_map[make_pair (string (filehash, MD5Length), sockclnt)] = FileChunk{};
    fileLog.writeLog (Log::INFO, string ("sendReqToClient end"));
    return 0;
}

// 向指定的客户端发送完成
int Uploader::sendDoneToClient (const string& filehash, socket_t sockclnt, bool immediate)
{
    fileLog.writeLog (Log::INFO, string ("sendDoneToClient begin"));
    int len;
    memcpy(this->uploadDone.MD5, filehash.data(), MD5Length);
    this->uploadDone.immediate = immediate;

    // set head
    headPacket.p = pType::UPLOAD_DONE;
    headPacket.len = PackageSizeMap.at (pType::UPLOAD_DONE);
    if (headlen != (len = write (sockclnt, &headPacket, headlen))) {
        fileLog.writeLog (Log::ERROR, string ("sendDoneToClient 发送head长度错误 len: ") + to_string (len));
        return -1;
    }

    if (headPacket.len != (len = write (sockclnt, &uploadDonePacket, sizeof(uploadDonePacket)))) {
        fileLog.writeLog (Log::ERROR, string ("sendDoneToClient 发送fetch包长度错误 len: ") + to_string (len));
        return -1;
    }
    // 到此，向client发送请求块完成

    // 修改socket状态
    if (uploadSet.count (sockclnt)) {
        uploadSet.erase (sockclnt);
    }
    else {
        fileLog.writeLog (Log::WARNING, string ("sendDoneToClient 目标socket先前不在upload集合中"));
    }
    // 改为idle状态
    idleSet.insert (sockclnt);

    // 依据md5和socket创建文件块映射
    fileChunk_map[make_pair (string (filehash, 32), sockclnt)] = FileChunk{};
    fileLog.writeLog (Log::INFO, string ("sendDoneToClient end"));
    return 0;

}

// 向客户端发送完成信号
// 复用UploadFetchBody，chunk=size
void Uploader::sendDoneToClient (const string& filehash)
{
    auto& fileinfo = fileMap.at (filehash);
    // 遍历file-socket表中的socket，发送完成信号
    for (auto& sock : fileinfo.sockSet) {
        sendDoneToClient(filehash, sock);
    }
}

// void Uploader::sendDoneToClient(socket_t sockclnt)
// {
    
// }
// 文件传输完成后的扫尾
void Uploader::uploadDone (const string& filehash)
{
    sendReqSaveFileInfotoDB (filehash);
    sendDoneToClient (filehash);
}

// 
int Uploader::run ()
{
    int epfd = epoll_create1 (0);
    if (epfd == -1) {
        fileLog.writeLog (Log::ERROR, string ("in run: epoll_create1 error") + strerror (errno));
        exit (-1);
    }
    epoll_event ep_events[MAXEVENTS];
    // EpollAdd (this->fifo_ctrl_r, EPOLLIN);
    EpollAdd (this->fifo_db_r, EPOLLIN);
    EpollAdd(this->socklisten, EPOLLIN);

    int epcnt;
    while (true) {
        epcnt = EpollWait ();

        for (int i = 0; i < epcnt; ++i) {
            auto& ep_ev = this->ep_events[i];
            // 从控制端来的包
            if (ep_ev.data.fd == fifo_ctrl_r) {
                // if (ep_ev.events & EPOLLERR) {
                //     // 如何加上fd对应的socket的详细信息？？？
                //     fileLog.writeLog (Log::ERROR, string ("EPOLLERR fifo_ctrl_r ") + " EpollDel!!!");
                //     EpollDel (ep_ev.data.fd);
                // }
                // if (ep_ev.events & EPOLLIN) {
                //     handleReqfromCtrl ();
                // }
            }
            // 从数据库模块来的包 
            else if (ep_ev.data.fd == fifo_db_r) {
                if (ep_ev.events & EPOLLIN) {
                    handlePacketFromDB ();
                }
            }
            // 建立新的连接
            else if(ep_ev.data.fd == socklisten){
                if(ep_ev.events & EPOLLERR){
                    fileLog.writeLog(Log::ERROR, string("EPOLLERR socket:")+to_string(socklisten));
                    EpollDel(ep_ev.data.fd);
                    exit(-1);
                }
                if(ep_ev.events & EPOLLIN){
                    int sockclnt = accept(socklisten, nullptr, nullptr);
                    if(sockclnt==-1){
                        fileLog.writeLog(Log::ERROR, string("accept error"));
                        exit(-1);
                    }
                    // 处理新连接
                    handleNewConnect(sockclnt);
                }
            }
            // 从client端来的包
            for (auto& sock:allSet){
                if(ep_ev.data.fd==sock){
                    if(ep_ev.events&EPOLLERR)                    {
                        fileLog.writeLog(Log::ERROR, string("EPOLLERR socket:") + to_string(sock));
                        exit(-1);
                    }
                    if(ep_ev.events&EPOLLIN){
                        handlePacketFromClient(sock);
                    }

                }
            }
            fileLog.writeLog (Log::WARNING, string ("there is a fd that was not handled"));
        }
    }






}
