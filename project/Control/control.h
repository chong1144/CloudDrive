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

#include "../Package/Package.h"
#include "../Utility/Log.h"
#include "../Utility/Config.h"

using std::map;
using std::pair;
using std::queue;
using std::string;
using std::to_string;
using std::vector;

// Session --- sock
using socketSession = map<string, uint32_t>;

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

    // �����һ�δ��������ļ��Ĺ��캯�� ����Ҫʹ�����к���
    // ���ʹ���޲ι��캯��������� ��Ҫ����ʹ�����к���

    // ��ʼ�� ������ȡ�����ļ� �����ܵ�
    void init(const string &configFile);
    // ���� server �� socket ����صĶ˿� ������ģʽ
    void startServer();
    // �ȴ��ͻ��˵����� ���ҷַ���ͬ�ı�ͷ����ͬ��ģ�� ������ģʽ ��ѭ��
    void waitForClient();


    // һЩ���ߺ���

    // �����ܵ�
    bool createPipe(const string &pipe);
    // ��� epoll ���ӵľ��
    void EpollAdd(const int &epfd, const int &fd, const uint32_t &events);
    // epoll �ȴ�����
    int EpollWait(const int &epfd, epoll_event* ep_events);
    // epoll ȥ�����
    void EpollDel(const int &epfd, const int &fd);
};
