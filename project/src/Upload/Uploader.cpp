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
    
    // ���õ�ַ����
    int opt = 1;
    ret = setsockopt(socklisten, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    if(ret==-1){
        fileLog.writeLog(Log::ERROR, string("Failed to setsockopt"));
        exit(-1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(this->port);

    // �󶨵�ַ
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

// ��DBģ�鷢�ͻ�ȡ�ļ���Ϣ������
// �˺��������յ�����CTRL���������ã�head�Լ�uploadreqbody��δ�䣬ֱ��ת������
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
// ��DBģ�鷢�ͱ����ļ���Ϣ������
int Uploader::sendReqSaveFileInfotoDB (const string& filehash)
{
    int len;
    auto& fileinfo = fileMap.at (filehash);
    strncpy(fileInfoPacket.md5, filehash.data(), MD5Length);
    fileInfoPacket.completed = isCompleted (filehash);
    // exist��sizeӦ�ò�Ӱ�����ݿ����Ϊ
    fileInfoPacket.exist = true;
    fileInfoPacket.size = fileinfo.size;

    headPacket.p = FILEINFO;
    // ������FileInfoBody�Ͳ�������bitmap���
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
        // �Ѿ���file-socket����
        if (sock == sockclnt) {
            fileLog.writeLog (Log::WARNING, string ("socket already exists") + to_string (sockclnt));
            return;
        }
    }
    fileLog.writeLog (Log::INFO, string ("add a new file-socket to fsTable"));
    fileMap[hashcode].sockSet.insert (sockclnt);

    fileLog.writeLog (Log::INFO, string ("addfsTable end") + to_string (sockclnt));
}

// �ж��ϴ����
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

// ������һ����λ�飬�����λ����Ϊsize������û�пտ飬��һ����ȫ�ϴ���ϣ�������
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

// // ����ӿ���ģ��������Ϣ��
// int Uploader::handleReqfromCtrl ()
// {
//     fileLog.writeLog (Log::INFO, string ("handleReqfromCtrl begin"));
//     int len;
//     int sockclnt;
//     // ��ȡheader
//     if ((len = read (fifo_ctrl_r, &headPacket, headlen)) != headlen) {
//         fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl ��ȡheader from Ctrl���ȴ��� len: ") + to_string (len));
//         return -1;
//     }

//     // ���ϴ��ļ�����
//     if (headPacket.p == pType::UPLOAD_REQ) {
//         fileLog.writeLog (Log::INFO, string ("handleReqfromCtrl ���� UPLOAD_REQ"));
//         if ((len = read (fifo_ctrl_r, &uploadReqPacket, headPacket.len)) != headPacket.len) {
//             fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl ��ȡreqBody from Ctrl���ȴ��� len: ") + to_string (len));
//             return -1;
//         }
//         // ���ս��յ��ϴ�����ת����databaseģ��
//         sendReqFileInfotoDB ();

//         // need a socket from ctrl!!!!!
//         if ((len = read (fifo_ctrl_r, &sockclnt, sizeof (int))) != 4) {
//             fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl ��ȡsocket from Ctrl���� len: ") + to_string (len));
//             return -1;
//         }
//         // ��socket�Ž��ȴ���ѯ����
//         fileLog.writeLog (Log::INFO, string ("handleReqfromCtrl �ȴ���ѯ���������sock: " + to_string (sockclnt)));
//         queryQue.push (sockclnt);

//         //// ���ļ���socket������ӵ�fs�� ��Ҫ�ȵ���ѯ���ݿ��������

//         return 0;
//     }
//     // ����Ҫ��Ӷ���ָͣ�����Ӧ��ȡ���Ƿ���Ҫ��
//     else {
//         fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl ���յ�����ctrl�Ĵ�������"));
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
        fileLog.writeLog(Log::ERROR, string("handleUploadReq ��ȡreqBody from Ctrl���ȴ��� len: ") + to_string(len));
        return -1;
    }
    // ���ս��յ��ϴ�����ת����databaseģ��
    sendReqFileInfotoDB();

    // ��socket�Ž��ȴ���ѯ����
    fileLog.writeLog(Log::INFO, string("handleUploadReq �ȴ���ѯ���������sock: " + to_string(sockclnt)));
    queryQue.push(sockclnt);

    // ����ӳ��
    sockFileMap[sockclnt] = string(uploadReqPacket.MD5, MD5Length);
    //// ���ļ���socket������ӵ�fs�� ��Ҫ�ȵ���ѯ���ݿ��������
    fileLog.writeLog(Log::INFO, string("handleUploadReq end"));
}


// ����push�ź�
int Uploader::handlePush (const string& filehash, socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("handlePush begin"));
    int len;
    if ((len = read (sockclnt, &pushPacket, headPacket.len)) != headPacket.len) {
        fileLog.writeLog (Log::ERROR, string ("handlePush ��ȡpushBody from client���ȴ��� len: ") + to_string (len));
        return -1;
    }
    auto& chunk = fileChunk_map[make_pair (filehash, sockclnt)];
    memcpy (chunk.content + chunk.size, pushPacket.content, pushPacket.len);
    chunk.size += pushPacket.len;
    if (pushPacket.last) {
        // �л���idle״̬
        uploadSet.erase(sockclnt);
        idleSet.insert(sockclnt);

        fileout.WriteFile (chunk);
        // �޸�λʾͼ
        fileMap.at (filehash).chunkBitMap[chunk.chunkNo] = CHUNK_EXIST;
        uint16_t chunkNo = getNextChunkNo (filehash);
        // �Ѿ�û����Ҫ�ϴ��Ŀ飬����fetch
        if (chunkNo==fileMap.at(filehash).size) {
            // �ж��ļ��Ƿ�����
            if (isCompleted (filehash)) {
                // �����������ɺ���ɨβ
                uploadDone (filehash);
            }
        }
        // ����Ҫ�ϴ��Ŀ�
        else {
            sendFetchToClient(filehash, chunkNo, sockclnt);
        }
        
        // д���ͷ��ļ���
        fileChunk_map.erase (make_pair (filehash, sockclnt));
        fileChunk_map[make_pair (filehash, sockclnt)] = FileChunk{};
    }
    fileLog.writeLog (Log::INFO, string ("handlePush end"));
    return 0;
}

// ��������ݿ�������Ϣ��
int Uploader::handlePacketFromDB ()
{
    char buf[64 * 1024];
    fileLog.writeLog (Log::INFO, string ("handlePacketFromDB begin"));
    int len;
    // read head from db
    len = read (fifo_db_r, &headPacket, headlen);
    if (len != headlen) {
        fileLog.writeLog (Log::ERROR, string ("handlePacketFromDB ��ȡheader from DB, ���ȴ��� len: ") + to_string (len));
        return -1;
    }
    // read fileinfo from db
    if(headPacket.p!=pType::FILEINFO){
        fileLog.writeLog(Log::ERROR, string("handlePacketFromDB �յ��İ�����FILEINFO"));
    }
    len = read (fifo_db_r, &fileInfoPacket, sizeof (fileInfoPacket));
    if (len != sizeof (fileInfoPacket)) {
        fileLog.writeLog (Log::ERROR, string ("handlePacketFromDB ��ȡfileinfo from DB, ���ȴ��� len: ") + to_string (len));
        return -1;
    }
    // �����յ�������fileInfoͷ
    fileLog.writeLog (Log::INFO, "handlePacketFromDB ��ȡfileInfo from DB done!");
    // read bitmap from db
    len = read (fifo_db_r, buf, headPacket.len - sizeof (fileInfoPacket));
    if (len != headPacket.len - sizeof (fileInfoPacket)) {
        fileLog.writeLog (Log::ERROR, string ("handlePacketFromDB ��ȡbitmap from DB, ���ȴ��� len: ") + to_string (len));
        return -1;
    }
    FileHash filehash (fileInfoPacket.md5, MD5Length);


    // �����յ�������bitmap
    fileLog.writeLog (Log::INFO, "handlePacketFromDB ��ȡbitmap from DB done!");
    fileLog.writeLog (Log::INFO, string ("filehash=") + filehash
        + ", exist=" + to_string (fileInfoPacket.exist)
        + ", completed=" + to_string (fileInfoPacket.completed)
        + ", size=" + to_string (fileInfoPacket.size));
    fileLog.writeLog (Log::INFO, string ("bitmap: ") + string (buf, fileInfoPacket.size));


    // �ж��ļ� ����&���� or ����&������
        // �ļ�����&����
    if (fileInfoPacket.completed && fileInfoPacket.exist) {
        // ������ɶ��У��봫
        fileLog.writeLog (Log::INFO, string ("handlePacketFromDB �봫! socket: ") + to_string (queryQue.front ()));
        
        sendDoneToClient(filehash, queryQue.front (), true);
        // doneQue.push (queryQue.front ());
        // �ӵȴ���ѯ������ɾ��
        queryQue.pop ();
    }
    // ����&������
    else if (~fileInfoPacket.completed) {
        fileLog.writeLog (Log::INFO, string ("handlePacketFromDB �ϴ�����ʼ") + to_string (queryQue.front ()));

        // �Ž�idle���ϣ�idle��Ϊ���ӵĳ�ʼ״̬
        idleSet.insert (queryQue.front ());
        // ����һ���µ�ӳ��md5-{bitmap, size, socketSet}
        FileLinker filelinker;
        filelinker.size = fileInfoPacket.size;
        filelinker.sockSet.insert (queryQue.front ());
        filelinker.chunkBitMap = move (string (buf, len));
        // insert pair, string ��copy constructorӦ��û����

        fileMap.insert (make_pair (filehash, filelinker));
        fileLog.writeLog (Log::INFO, string ("handlePacketFromDB ���md5-{bitmap, size, socketSet}ӳ��") + to_string (queryQue.front ()));
        uint16_t chunkNo = getNextChunkNo (filehash);
        // �Ѿ�������������chunkNo==size�ж�������

        // ��client����chunk����sockתΪupload״̬
        sendFetchToClient (filehash, chunkNo, queryQue.front ());
        queryQue.pop ();
    }
    else {
        fileLog.writeLog (Log::WARNING, string ("handlePacketFromDB fileInfoδ��������") + to_string (queryQue.front ()));
    }
    // ֻҪ�����ݿⷢ��UPLOAD���󣬲����ھͱ�������һ�����ڣ����ؿ��ǲ�����

    fileLog.writeLog (Log::INFO, string ("handlePacketFromDB end"));
    return 0;
}

// ���ղ���������Client��packet
int Uploader::handlePacketFromClient(socket_t sockclnt)
{
    fileLog.writeLog (Log::INFO, string ("handlePacketFromClient begin"));
    int len;
    if ((len = read (fifo_ctrl_r, &headPacket, headlen)) != headlen) {
        fileLog.writeLog (Log::ERROR, string ("handlePacketFromClient ��ȡheader from client���ȴ��� len: ") + to_string (len));
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
    // ʣ����ͣ�ͻָ�����û���
    fileLog.writeLog(Log::INFO, string("handlePacketFromClient end"));
    return 0;
}

// �����µ�����
// ����״̬Ϊidle
// ���EPOLL����
int Uploader::handleNewConnect(socket_t sockclnt)
{
    // �µ����ӣ�����idle
    idleSet.insert(sockclnt);
    allSet.insert(sockclnt);
    // ��Ӽ���
    EpollAdd(sockclnt, EPOLLIN);

    return 0;
}

// ��Client����������
// �޸�socket״̬Ϊupload
// ����(md5, socket)-->�����ļ���ӳ��
// ÿ�ֲ����п���socketʱ����
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
        fileLog.writeLog (Log::ERROR, string ("sendReqToClient ����head���ȴ��� len: ") + to_string (len));
        return -1;
    }

    if (headPacket.len != (len = write (sockclnt, &fetchPacket, headPacket.len))) {
        fileLog.writeLog (Log::ERROR, string ("sendReqToClient ����fetch�����ȴ��� len: ") + to_string (len));
        return -1;
    }
    // ���ˣ���client������������

    // �޸�λʾͼ
    fileMap[filehash].chunkBitMap[chunkNo] = CHUNK_UPLOADING;

    // �޸�socket״̬
    if (idleSet.count (sockclnt)) {
        idleSet.erase (sockclnt);
    }
    else {
        fileLog.writeLog (Log::WARNING, string ("sendReqToClient Ŀ��socket��ǰ����idle������"));
    }
    // ��Ϊ���ڴ���״̬
    uploadSet.insert (sockclnt);

    // ����md5��socket�����ļ���ӳ��
    fileChunk_map[make_pair (string (filehash, MD5Length), sockclnt)] = FileChunk{};
    fileLog.writeLog (Log::INFO, string ("sendReqToClient end"));
    return 0;
}

// ��ָ���Ŀͻ��˷������
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
        fileLog.writeLog (Log::ERROR, string ("sendDoneToClient ����head���ȴ��� len: ") + to_string (len));
        return -1;
    }

    if (headPacket.len != (len = write (sockclnt, &uploadDonePacket, sizeof(uploadDonePacket)))) {
        fileLog.writeLog (Log::ERROR, string ("sendDoneToClient ����fetch�����ȴ��� len: ") + to_string (len));
        return -1;
    }
    // ���ˣ���client������������

    // �޸�socket״̬
    if (uploadSet.count (sockclnt)) {
        uploadSet.erase (sockclnt);
    }
    else {
        fileLog.writeLog (Log::WARNING, string ("sendDoneToClient Ŀ��socket��ǰ����upload������"));
    }
    // ��Ϊidle״̬
    idleSet.insert (sockclnt);

    // ����md5��socket�����ļ���ӳ��
    fileChunk_map[make_pair (string (filehash, 32), sockclnt)] = FileChunk{};
    fileLog.writeLog (Log::INFO, string ("sendDoneToClient end"));
    return 0;

}

// ��ͻ��˷�������ź�
// ����UploadFetchBody��chunk=size
void Uploader::sendDoneToClient (const string& filehash)
{
    auto& fileinfo = fileMap.at (filehash);
    // ����file-socket���е�socket����������ź�
    for (auto& sock : fileinfo.sockSet) {
        sendDoneToClient(filehash, sock);
    }
}

// void Uploader::sendDoneToClient(socket_t sockclnt)
// {
    
// }
// �ļ�������ɺ��ɨβ
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
            // �ӿ��ƶ����İ�
            if (ep_ev.data.fd == fifo_ctrl_r) {
                // if (ep_ev.events & EPOLLERR) {
                //     // ��μ���fd��Ӧ��socket����ϸ��Ϣ������
                //     fileLog.writeLog (Log::ERROR, string ("EPOLLERR fifo_ctrl_r ") + " EpollDel!!!");
                //     EpollDel (ep_ev.data.fd);
                // }
                // if (ep_ev.events & EPOLLIN) {
                //     handleReqfromCtrl ();
                // }
            }
            // �����ݿ�ģ�����İ� 
            else if (ep_ev.data.fd == fifo_db_r) {
                if (ep_ev.events & EPOLLIN) {
                    handlePacketFromDB ();
                }
            }
            // �����µ�����
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
                    // ����������
                    handleNewConnect(sockclnt);
                }
            }
            // ��client�����İ�
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
