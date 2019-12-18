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
#define FileIOPath "../config.conf"
using std::set;
using std::pair;
using std::make_pair;
using std::to_string;
using size_t = uint64_t;
using pType = PackageType;
const size_t headlen = sizeof (UniformHeader);
using std::queue;
const int MAXEVENTS = 20;

const char CHUNK_EXIST = '1';
const char CHUNK_NOEXIST = '0';
const char CHUNK_UPLOADING = '2';

int epoll_add (int epfd, int fd, uint32_t events)
{
	static epoll_event ep_ev;
	ep_ev.data.fd = fd;
	ep_ev.events = events;
	if (-1 == epoll_ctl (epfd, EPOLL_CTL_ADD, ep_ev.data.fd, &ep_ev)) {
		perror ("epoll_ctl()");
		return -1;
	}
	return 0;
}

int epoll_del (int epfd, int fd)
{
	if (-1 == epoll_ctl (epfd, EPOLL_CTL_DEL, fd, nullptr)) {
		perror ("epoll_ctl()");
		return -1;
	}
	return 0;
}

class Uploader
{
	using FileHash = string;
	using socket_t = int;
	enum class SOCKETSTATE :int
	{
		IDEL, UPLOADING
	};
	struct SocketState
	{
		socket_t sock;
		SOCKETSTATE state;
	};
	struct FileLinker
	{
		uint32_t size;					// ��¼chunk����
		string chunkBitMap;				// �ļ�λʾͼ
		set <socket_t> sockSet;			// �����ϴ����ļ������Ӽ���
	};
	// ά����md5-{bitmap, size, socketSet}�� 
	// �յ����ݿ�Ĳ�ѯ������ļ�������ʱ����������������ͬһ���ļ�ʱ���
	// ������ͣʱ���Ͽ�ʱ��ɾ��������
	// ����������ӶϿ����ļ��ϴ����ʱд�����ݿ⣬�����˱�ɾ��
	map<FileHash, FileLinker> fileMap;
	// �ļ�hash&socket��Ӧһ���ļ���
	// ��ͻ��˷�������sendReqToClientʱ������
	// ����һ���ļ��鲢д����̺�����
	map<pair<FileHash, socket_t >, FileChunk> fileChunk_map;
	// �ȷ����������ȼ�¼��query�����У��ȴ����ݿⷵ���ļ���Ϣ
	// �յ�CTRL���ϴ�������ӣ��յ����ݿⷢ�����ļ���Ϣ��ɾ��
	queue<socket_t> queryQue;
	// 
	// �յ��ļ���Ϣ������������ӣ����ӵ�Ĭ��״̬
	// �����ʼ�������ݣ���ת�浽uploadSet�������ͣ����ת�浽pause����
	// �����жϣ��ļ����������ɾ��
	set<socket_t> idleSet;
	// ��client�����������Ϣ����ӣ�
	// ��ͣ����ת�浽pause���ϣ�
	// ÿ�������ɣ���ת�浽idle���ϣ�
	// �ļ�������ϣ���ת�浽doneQue��
	set<socket_t> uploadSet;
	// �յ���ͣ�������
	// �յ�����������ת�浽idle����
	set<socket_t> pauseSet;
	// ��ɵĴ��͵�client����Ϊһ��clientͬʱֻ�ܴ���һ���ļ�����client��ɼ����ļ�������� 
	// ��uploadSetת��
	// ��client��������źŰ���ɾ��
	queue<socket_t> doneQue;

	//database model to/from upload model
	int fifo_db_w;
	int fifo_db_r;

	//control model to/from upload model
	int fifo_ctrl_w;
	int fifo_ctrl_r;

	//readfile model to/from upload model
	//int fifo_readfile_w;
	//int fifo_readfile_r;


	UniformHeader headPacket;
	UploadReqBody uploadReqPacket;
	UploadRespBody respPacket;
	UploadFetchBody fetchPacket;
	UploadPushBody pushPacket;
	FileInfoBody fileInfoPacket;
	Log fileLog;
	//config
	Config config;
	// epollfd
	int epfd;
	epoll_event ep_events[MAXEVENTS];

	// listen socket
	int socklisten;
	FileOperations fileout;

	uint32_t port;
    string ip;
    uint8_t numListen;
    uint8_t maxEvent;
	uint32_t sock;
    sockaddr_in addr;

public:
	Uploader (string config_file, string log_file) :config (config_file), fileLog (log_file), fileout (FileIOPath){};

	void init(const string&);
    
	void startServer();

	void EpollAdd (int fd, uint32_t events);

	int EpollWait ();

	void EpollDel (int fd);

	// ��DBģ�鷢�ͻ�ȡ�ļ���Ϣ������
	// �˺��������յ�����CTRL���������ã�head�Լ�uploadreqbody��δ�䣬ֱ��ת������
	int sendReqFileInfotoDB ();
	// ��DBģ�鷢�ͱ����ļ���Ϣ������
	int sendReqSaveFileInfotoDB (const string& filehash);

	void addfsTable (FileHash hashcode, socket_t sockclnt);

	// �ж��ϴ����
	bool isCompleted (const FileHash& filehash);

	// ������һ����λ�飬�����λ����Ϊsize������û�пտ飬��һ����ȫ�ϴ���ϣ�������
	uint16_t getNextChunkNo (const FileHash& filehash);
	// // ����ӿ���ģ��������Ϣ��
	// int handleReqfromCtrl ();

	// �����client�������ݰ�
	int handlePacketFromClient (const string& filehash, socket_t sockclnt);
	// ��������ݿ�������Ϣ��
	int handlePacketFromDB ();

	// �����µ�����������
	int handleNewConnect();
	
	// ��Client����������
	// �޸�socket״̬Ϊupload
	// ����(md5, socket)-->�����ļ���ӳ��
	// ÿ�ֲ����п���socketʱ����
	int sendReqToClient (const string& filehash, uint16_t chunkNo, socket_t sockclnt);
	
	// ��ͻ��˷�������ź�
	// ����UploadFetchBody��chunk=size
	void sendDoneToClient (const string& filehash);
	// �ļ�������ɺ��ɨβ
	void uploadDone (const string& filehash);

	// 
	int run ();



};


