#include "../Package/Package.h"
#include "../Utility/Config.h"
#include "../Utility/Log.h"
#include "../filesystem/FileOperations.h"
#include <iostream>
#include <queue>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <fcntl.h>
#include <sys/socket.h>

#include <openssl/md5.h>

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
		uint32_t size;					// 记录chunk数量
		string chunkBitMap;				// 文件位示图
		set <socket_t> sockSet;			// 正在上传此文件的链接集合
	};
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

	//database model to/from upload model
	int fifo_db_w;
	int fifo_db_r;

	//control model to/from upload model
	int fifo_ctrl_w;
	int fifo_ctrl_r;

	//readfile model to/from upload model
	//int fifo_readfile_w;
	//int fifo_readfile_r;


	UniformHeader head;
	UploadReqBody uploadReq;
	UploadRespBody resp;
	UploadFetchBody fetch;
	UploadPushBody pushPacket;
	FileInfoBody fileInfo;
	Log fileLog;
	//config
	Config config;
	// epollfd
	int epfd;
	epoll_event ep_events[MAXEVENTS];
	FileOperations fileout;

public:
	Uploader (string config_file, string log_file) :config (config_file), fileLog (log_file), fileout (FileIOPath)
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

	void EpollAdd (int fd, uint32_t events)
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

	int EpollWait ()
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

	void EpollDel (int fd)
	{
		fileLog.writeLog (Log::INFO, "Epolldel");
		if (-1 == epoll_ctl (epfd, EPOLL_CTL_DEL, fd, nullptr)) {
			perror ("epoll_ctl()");
			fileLog.writeLog (Log::ERROR, string ("epoll_ctl()") + strerror (errno));
		}
	}

	// 向DB模块发送获取文件信息的请求
	// 此函数仅在收到来自CTRL的请求后调用，head以及uploadreqbody都未变，直接转发即可
	int sendReqFileInfotoDB ()
	{
		int len;
		len = write (fifo_db_w, &head, headlen);
		if (len != headlen) {
			fileLog.writeLog (Log::ERROR, string ("sendReqFileInfotoDB() write() ret val=") + to_string (len));
			return -1;
		}

		len = write (fifo_db_w, &uploadReq, sizeof (uploadReq));
		if (len != sizeof (uploadReq)) {
			fileLog.writeLog (Log::ERROR, string ("sendReqFileInfotoDB() write() ret val=") + to_string (len));
			return -1;
		}

		return 0;
	};
	// 向DB模块发送保存文件信息的请求
	int sendReqSaveFileInfotoDB ()
	{
		
	}

	void addfsTable (FileHash hashcode, socket_t sockclnt)
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
	bool isCompleted (const FileHash& filehash)
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
	uint16_t getNextChunkNo (const FileHash& filehash)
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

	// 处理从控制模块来的信息包
	int handleReqfromCtrl ()
	{
		fileLog.writeLog (Log::INFO, string ("handleReqfromCtrl begin"));
		int len;
		int sockclnt;
		// 读取header
		if ((len = read (fifo_ctrl_r, &head, headlen)) != headlen) {
			fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl 读取header from Ctrl长度错误 len: ") + to_string (len));
			return -1;
		}

		// 是上传文件请求
		if (head.p == pType::UPLOAD_REQ) {
			fileLog.writeLog (Log::INFO, string ("handleReqfromCtrl 处理 UPLOAD_REQ"));
			if ((len = read (fifo_ctrl_r, &uploadReq, head.len)) != head.len) {
				fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl 读取reqBody from Ctrl长度错误 len: ") + to_string (len));
				return -1;
			}
			// 将刚接收的上传请求转发给database模块
			sendReqFileInfotoDB ();

			// need a socket from ctrl!!!!!
			if ((len = read (fifo_ctrl_r, &sockclnt, sizeof (int))) != 4) {
				fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl 读取socket from Ctrl错误 len: ") + to_string (len));
				return -1;
			}
			// 将socket放进等待查询集合
			fileLog.writeLog (Log::INFO, string ("handleReqfromCtrl 等待查询集合里添加sock: " + to_string (sockclnt)));
			queryQue.push (sockclnt);

			//// 将文件和socket关联添加到fs表 需要等到查询数据库完成再做

			return 0;
		}
		// 还需要添加对暂停指令的响应，取消是否需要？
		else {
			fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl 接收到来自ctrl的错误请求"));
			return -1;
		}
	}
	// 处理从client来的数据包
	int handlePacketFromClient (const string& filehash, socket_t sockclnt)
	{
		fileLog.writeLog (Log::INFO, string ("handlePacketFromClient begin"));
		int len;
		if ((len = read (fifo_ctrl_r, &head, headlen)) != headlen) {
			fileLog.writeLog (Log::ERROR, string ("handlePacketFromClient 读取header from client长度错误 len: ") + to_string (len));
			return -1;
		}
		if (head.p == pType::UPLOAD_PUSH) {
			if ((len = read (fifo_ctrl_r, &pushPacket, head.len)) != head.len) {
				fileLog.writeLog (Log::ERROR, string ("handlePacketFromClient 读取pushBody from client长度错误 len: ") + to_string (len));
				return -1;
			}
			// already get right package

			// 
			auto& chunk = fileChunk_map[make_pair (filehash, sockclnt)];
			memcpy (chunk.content + chunk.size, pushPacket.content, pushPacket.len);
			chunk.size += pushPacket.len;
			if (pushPacket.last) {
				fileout.WriteFile (chunk);
				// 修改位示图
				fileMap.at (filehash).chunkBitMap[chunk.chunkNo] = CHUNK_EXIST;
				uint16_t chunkNo = getNextChunkNo (filehash);
				// 已经没有空闲块，无需fetch
				if (chunkNo==fileMap.at(filehash).size) {
					// 判断文件是否完整
					if (isCompleted (filehash)) {
						
					}
				}
				else {

				}
				// 写完释放文件块


				fileChunk_map.erase (make_pair (filehash, sockclnt));
				fileChunk_map[make_pair (filehash, sockclnt)] = FileChunk{};
			}
		}
		fileLog.writeLog (Log::INFO, string ("handlePacketFromClient end"));
	}
	// 处理从数据库来的信息包
	int handlePacketFromDB ()
	{
		char buf[64 * 1024];
		fileLog.writeLog (Log::INFO, string ("handlePacketFromDB begin"));
		int len;
		// read head from db
		len = read (fifo_db_r, &head, headlen);
		if (len != headlen) {
			fileLog.writeLog (Log::ERROR, string ("handlePacketFromDB 读取header from DB, 长度错误 len: ") + to_string (len));
			return -1;
		}
		// read fileinfo from db
		len = read (fifo_db_r, &fileInfo, sizeof (fileInfo));
		if (len != sizeof (fileInfo)) {
			fileLog.writeLog (Log::ERROR, string ("handlePacketFromDB 读取fileinfo from DB, 长度错误 len: ") + to_string (len));
			return -1;
		}
		// 到此收到完整的fileInfo头
		fileLog.writeLog (Log::INFO, "handlePacketFromDB 读取fileInfo from DB done!");
		// read bitmap from db
		len = read (fifo_db_r, buf, head.len - sizeof (fileInfo));
		if (len != head.len - sizeof (fileInfo)) {
			fileLog.writeLog (Log::ERROR, string ("handlePacketFromDB 读取bitmap from DB, 长度错误 len: ") + to_string (len));
			return -1;
		}
		string filehash (fileInfo.md5, 32);


		// 到此收到完整的bitmap
		fileLog.writeLog (Log::INFO, "handlePacketFromDB 读取bitmap from DB done!");
		fileLog.writeLog (Log::INFO, string ("filehash=") + filehash
			+ ", exist=" + to_string (fileInfo.exist)
			+ ", completed=" + to_string (fileInfo.completed)
			+ ", size=" + to_string (fileInfo.size));
		fileLog.writeLog (Log::INFO, string ("bitmap: ") + string (buf, fileInfo.size));


		// 判断文件 存在&完整 or 存在&不完整
			// 文件存在&完整
		if (fileInfo.completed && fileInfo.exist) {
			// 加入完成队列，秒传
			fileLog.writeLog (Log::INFO, string ("handlePacketFromDB 秒传! socket: ") + to_string (queryQue.front ()));
			doneQue.push (queryQue.front ());
			// 从等待查询队列里删除
			queryQue.pop ();
		}
		// 存在&不完整
		else if (~fileInfo.completed) {
			fileLog.writeLog (Log::INFO, string ("handlePacketFromDB 上传任务开始") + to_string (queryQue.front ()));

			// 放进idle集合，idle作为连接的初始状态
			idleSet.insert (queryQue.front ());
			// 建立一个新的映射md5-{bitmap, size, socketSet}
			FileLinker filelinker;
			filelinker.size = fileInfo.size;
			filelinker.sockSet.insert (queryQue.front ());
			filelinker.chunkBitMap = move (string (buf, len));
			// insert pair, string 有copy constructor应该没问题
			fileMap.insert (make_pair (filehash, filelinker));
			fileLog.writeLog (Log::INFO, string ("handlePacketFromDB 添加md5-{bitmap, size, socketSet}映射") + to_string (queryQue.front ()));
			uint16_t chunkNo = getNextChunkNo (filehash);
			// 已经不完整，不用chunkNo==size判断完整性

			// 向client发送chunk请求，sock转为upload状态
			sendReqToClient (filehash, chunkNo, queryQue.front ());
			queryQue.pop ();
		}
		else {
			fileLog.writeLog (Log::WARNING, string ("handlePacketFromDB fileInfo未处理的情况") + to_string (queryQue.front ()));
		}
		// 只要向数据库发出UPLOAD请求，就一定存在，不必考虑不存在

		fileLog.writeLog (Log::INFO, string ("handlePacketFromDB end"));
		return 0;
	}

	// 向Client发送请求块号
	// 修改socket状态为upload
	// 建立(md5, socket)-->单个文件块映射
	// 每轮查找有空闲socket时调用
	int sendReqToClient (const string& filehash, uint16_t chunkNo, socket_t sockclnt)
	{
		fileLog.writeLog (Log::INFO, string ("sendReqToClient begin"));
		int len;
		memcpy (this->fetch.MD5, filehash.data (), 32);
		fetch.chunkNo = chunkNo;
		// set head
		head.p = UPLOAD_FETCH;
		head.len = PackageSizeMap.at (UPLOAD_FETCH);
		if (headlen != (len = write (sockclnt, &head, headlen))) {
			fileLog.writeLog (Log::ERROR, string ("sendReqToClient 发送head长度错误 len: ") + to_string (len));
			return -1;
		}

		if (head.len != (len = write (sockclnt, &fetch, head.len))) {
			fileLog.writeLog (Log::ERROR, string ("sendReqToClient 发送fetch包长度错误 len: ") + to_string (len));
			return -1;
		}
		// 到此，向client发送请求块完成

		// 修改socket状态
		if (idleSet.count (sockclnt)) {
			idleSet.erase (sockclnt);
		}
		else {
			fileLog.writeLog (Log::WARNING, string ("sendReqToClient 目标socket先前不在idel集合中"));
		}
		// 改为正在传输状态
		uploadSet.insert (sockclnt);

		// 依据md5和socket创建文件块映射
		fileChunk_map[make_pair (string (filehash, 32), sockclnt)] = FileChunk{};
		fileLog.writeLog (Log::INFO, string ("sendReqToClient end"));
		return 0;
	}
	
	// 向客户端发送完成信号
	// 复用UploadFetchBody，chunk=size
	void sendDoneToClient (const string& filehash)
	{
		auto& fileinfo = fileMap.at (filehash);
		// 遍历file-socket表中的socket，发送完成信号
		for (auto& sock : fileinfo.sockSet) {
			sendReqToClient (filehash, fileinfo.size, sock);
		}
	}

	// 文件传输完成后的扫尾
	void uploadDone (const string& filehash)
	{
		
	}

	// 
	int run ()
	{
		int epfd = epoll_create1 (0);
		if (epfd == -1) {
			fileLog.writeLog (Log::ERROR, string ("in run: epoll_create1 error") + strerror (errno));
			exit (-1);
		}
		epoll_event ep_events[MAXEVENTS];
		// 初始监视控制模块的信号
		EpollAdd (this->fifo_ctrl_r, EPOLLIN);
		EpollAdd (this->fifo_db_r, EPOLLIN);

		int epcnt;
		while (true) {
			epcnt = EpollWait ();

			for (int i = 0; i < epcnt; ++i) {
				auto& ep_ev = this->ep_events[i];
				// 从控制端来的包
				if (ep_ev.data.fd == fifo_ctrl_r) {
					if (ep_ev.events & EPOLLERR) {
						// 如何加上fd对应的socket的详细信息？？？
						fileLog.writeLog (Log::ERROR, string ("EPOLLERR fifo_ctrl_r ") + " EpollDel!!!");
						EpollDel (ep_ev.data.fd);
					}
					if (ep_ev.events & EPOLLIN) {
						handleReqfromCtrl ();
					}
				}
				// 从数据库模块来的包 
				if (ep_ev.data.fd == fifo_db_r) {
					if (ep_ev.events & EPOLLIN) {
						handlePacketFromDB ();
					}
				}
				// 从client端来的包
				for (auto& file : fileMap) {
					for (auto& sock : file.second.sockSet) {
						if (sock == ep_ev.data.fd) {
							// 错误，暂时的处理是删除句柄监视
							if (ep_ev.events & EPOLLERR) {
								fileLog.writeLog (Log::ERROR, string ("EPOLLERR sockclnt: ") + to_string (ep_ev.data.fd) + " EpollDel!!!");
								EpollDel (ep_ev.data.fd);
							}

							if (ep_ev.events & EPOLLIN) {
								handlePacketFromClient (file.first, ep_ev.data.fd);
							}
						}
					}
				}
				fileLog.writeLog (Log::WARNING, string ("there is a fd that was not handled"));

			}
		}






	}


};


