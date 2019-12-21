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
using FileHash = string;
using socket_t = int;

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

struct FileLinker
{
	uint32_t size;					// 记录chunk数量
	string chunkBitMap;				// 文件位示图
	set <socket_t> sockSet;			// 正在上传此文件的链接集合
	FileLinker& operator=(const FileLinker& _Right){
		this->size = _Right.size;
		this->chunkBitMap = _Right.chunkBitMap;
		this->sockSet = _Right.sockSet;
	}
};

class Uploader
{
	// enum class SOCKETSTATE :int
	// {
	// 	IDEL, UPLOADING
	// };
	// struct SocketState
	// {
	// 	socket_t sock;
	// 	SOCKETSTATE state;
	// };

	// 维护的md5-{bitmap, size, socketSet}表 
	// 收到数据库的查询结果，文件不完整时创建，新连接请求同一个文件时添加
	// 连接暂停时，断开时，删除该连接
	// 所有相关连接断开，文件上传完成时写入数据库，并将此表删除
	map<FileHash, FileLinker> fileMap;
	// 文件hash&socket对应一个文件块
	// 向客户端发送请求sendReqToClient时，创建
	// 收齐一个文件块并写入磁盘后，销毁
	map<pair<FileHash, socket_t >, FileChunk> fileChunk_map;
	// 先发来的请求先记录在query队列中，等待数据库返回文件信息
	// 收到CTRL的上传请求添加，收到数据库发来的文件信息则删除
	queue<socket_t> queryQue;
	// 
	// 收到文件信息，不完整则添加，连接的默认状态
	// 如果开始发送数据，则转存到uploadSet，如果暂停，则转存到pause集合
	// 连接中断，文件传输完成则删除
	set<socket_t> idleSet;
	// 向client发起请求块信息后添加，
	// 暂停，则转存到pause集合，
	// 每块接收完成，则转存到idle集合，
	// 文件接收完毕，则转存到doneQue中
	set<socket_t> uploadSet;
	// 收到暂停请求添加
	// 收到继续请求，则转存到idle集合
	set<socket_t> pauseSet;
	// 完成的传送的client，因为一个client同时只能传送一个文件，故client完成即该文件传输完成 
	// 从uploadSet转存
	// 向client发送完成信号包则删除
	queue<socket_t> doneQue;

	// 目前连接的全集
	set<socket_t> allSet;

	map<socket_t, FileHash> sockFileMap;
	//database model to/from upload model
	int fifo_db_w;
	int fifo_db_r;

	//control model to/from upload model
	int fifo_ctrl_w;
	int fifo_ctrl_r;

	// 以下各个包，统一创建，公共使用
	UniformHeader headPacket;
	UploadReqBody uploadReqPacket;
	UploadRespBody respPacket;
	UploadFetchBody fetchPacket;
	UploadPushBody pushPacket;
	UploadDoneBody uploadDonePacket;
	FileInfoBody fileInfoPacket;
	
	// log module
	Log fileLog;
	// config
	Config config;
	// write module
	FileOperations fileout;

	// epollfd
	int epfd;
	epoll_event ep_events[MAXEVENTS];

	// listen socket
	int socklisten;

	uint32_t port;
    string ip;
    uint8_t numListen;
    uint8_t maxEvent;
    sockaddr_in addr;

public:
	Uploader(const string& config_file);

	void openfifo();

	void init(const string&);
    
	void startServer();

	void EpollAdd (int fd, uint32_t events);

	int EpollWait ();

	void EpollDel (int fd);

	// 向DB模块发送获取文件信息的请求
	// 此函数仅在收到来自客户端的上传请求后调用，head以及uploadreqbody都未变，直接转发即可
	int sendReqFileInfotoDB ();
	// 向DB模块发送保存文件信息的请求
	int sendReqSaveFileInfotoDB (const string& filehash);

	void addfsTable (FileHash hashcode, socket_t sockclnt);

	// 判断上传完成
	bool isCompleted (const FileHash& filehash);

	// 返回下一个空位块，如果空位块编号为size，表明没有空块，不一定完全上传完毕！！！！
	uint16_t getNextChunkNo (const FileHash& filehash);
	// // 处理从控制模块来的信息包
	// int handleReqfromCtrl ();

	int handleUploadReq(socket_t sockclnt);

	// 处理push
	int handlePush (const string& filehash, socket_t sockclnt);
	// 处理从数据库来的信息包
	int handlePacketFromDB ();

	int handlePacketFromClient(socket_t);
	// 处理新到的连接请求
	int handleNewConnect(socket_t sockclnt);
	
	// 向Client发送请求块号
	// 修改socket状态为upload
	// 建立(md5, socket)-->单个文件块映射
	// 每轮查找有空闲socket时调用
	int sendFetchToClient (const string& filehash, uint16_t chunkNo, socket_t sockclnt);
	
	int sendDoneToClient (const string& filehash, socket_t sockclnt, bool immediate=false);
	// 向客户端发送完成信号
	// 复用UploadFetchBody，chunk=size
	void sendDoneToClient (const string& filehash);
	// 文件传输完成后的扫尾
	void uploadDone (const string& filehash);

	// 
	int run ();



};


