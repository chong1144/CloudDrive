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
	// �ȷ����������ȼ�¼��query�����У��ȴ����ݿⷵ����Ϣ
	// �յ�client������ӣ��յ����ݿⷢ������Ϣ��ɾ��
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
    // �ȹ̶�
    epoll_event ep_events[MAXEVENTS];
    int socklisten;

    


public:
    CTRL();
    CTRL(const string& configFile);
    ~CTRL();

    // ��ʼ�� ������ȡ�����ļ� �����ܵ�
    void init(const string &configFile);
    // ���� server �� socket ����صĶ˿� ������ģʽ
    void startServer();
    // �ȴ��ͻ��˵����� ���ҷַ���ͬ�ı�ͷ����ͬ��ģ�� ������ģʽ ��ѭ��
    void waitForClient();

    void openfifo();

    int handleNewConnection(socket_t sockclnt);
    // ��������ݿ�����packet
    int handlePacketFromDB ();
    int handleSigninResFromDB();
    int handleSignupResFromDB();
    int handleCopyRespFromDB();
    int handleDeleteRespFromDB();
    int handleMkdirRespFromDB();
    int handleMoveRespFromDB();
    int handleSynRespFromDB();

    // �����Client����packet
    int handlePacketFromClient(socket_t sockclnt);
    int handleSigninFromClient(socket_t sockclnt);
    int handleSignupFromClient(socket_t sockclnt);
    int handleCopyFromClient(socket_t sockclnt);
    int handleDeleteFromClient(socket_t sockclnt);
    int handleMkdirFromClient(socket_t sockclnt);
    int handleMoveFromClient(socket_t sockclnt);
    int handleSynReqFromClient(socket_t sockclnt);

    // ��Client���ͻذ�
    int sendSignupResToClient(socket_t sockclnt);
    int sendSigninResToClient(socket_t sockclnt);
    int sendCopyRespToCilent(socket_t sockclnt);
    int sendDeleteRespToCilent(socket_t sockclnt);
    int sendMkdirRespToCilent(socket_t sockclnt);
    int sendMoveRespToCilent(socket_t sockclnt);
    int sendSynRespToCilent(socket_t sockclnt);
    // �����ݿⷢ������
    int sendSigninToDB();
    int sendSignupToDB();
    int sendCopyToDB();
    int sendDeleteToDB();
    int sendMkdirToDB();
    int sendMoveToDB();
    int sendSynReqToDB();    
    // �����ܵ�
    bool createPipe(const string &pipe);
    // ��� epoll ���ӵľ��
	void EpollAdd (int fd, uint32_t events);

	int EpollWait ();

	void EpollDel (int fd);

    void handleDisconnection(socket_t);

    // ��ȡ socket �� ip �� �˿�
    bool GetIPPort(const uint32_t &sock, string &ip, uint16_t &port);
    
    void run();
};