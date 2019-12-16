#include "../Package/Package.h"
#include "../Utility/Config.h"
#include "../Utility/Log.h"
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

using std::to_string;
using size_t = uint64_t;
using pType = PackageType;
const size_t headlen = sizeof (UniformHeader);
using std::queue;
const int MAXEVENTS = 20;

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
	using fileHash = string;
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
	// 维护的file-socket表
	map<fileHash, vector<SocketState>> fs_map;

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
	UploadReqBody req;
	UploadRespBody resp;
	UploadFetchBody fetch;
	UploadPushBody push;
	Log fileLog;
	//config
	Config config;
	// epollfd
	int epfd;
	epoll_event ep_events[MAXEVENTS];


public:
	Uploader (string config_file, string log_file) :config (config_file), fileLog (log_file)
	{
		epfd = epoll_create1 (0);
		if (epfd == -1) {
			fileLog.writeLog (Log::ERROR, string ("in run: epoll_create1 error") + strerror (errno));
			exit (-1);
		}
		//open fifo
		fifo_db_w = open (config.getValue ("Global", "path_fifo_rtod").c_str (), O_WRONLY);
		fifo_db_r = open (config.getValue ("Global", "path_fifo_dtor").c_str (), O_RDONLY);
		fifo_ctrl_w = open (config.getValue ("Global", "path_fifo_rtoc").c_str (), O_WRONLY);
		fifo_ctrl_r = open (config.getValue ("Global", "path_fifo_ctor").c_str (), O_RDONLY);
	}

	// 向DB模块发送获取文件信息的请求
	int sendReqFileInfotoDB (fileHash hashcode)
	{

	};

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
		fileLog.writeLog (Log::INFO, string ("epollwait end, cnt: ") + to_string (cnt));
		return cnt;
	}

	void Epolldel (int fd)
	{
		fileLog.writeLog (Log::INFO, "Epolldel");
		if (-1 == epoll_ctl (epfd, EPOLL_CTL_DEL, fd, nullptr)) {
			perror ("epoll_ctl()");
			fileLog.writeLog (Log::ERROR, string ("epoll_ctl()") + strerror (errno));
		}
	}

	void addfsTable (fileHash hashcode, socket_t sockclnt)
	{
		fileLog.writeLog (Log::INFO, string ("addfsTable begin") + to_string (sockclnt));
		for (auto sock : fs_map[hashcode]) {
			// 已经在file-socket表中
			if (sock.sock == sockclnt) {
				fileLog.writeLog (Log::WARNING, string ("socket already exists") + to_string (sockclnt));
				return;
			}
		}
		fileLog.writeLog (Log::INFO, string ("add a new file-socket to fsTable"));
		fs_map[hashcode].push_back ({ sockclnt,SOCKETSTATE::IDEL });
		fileLog.writeLog (Log::INFO, string ("addfsTable end") + to_string (sockclnt));
	}

	// 从控制模块获取上传请求
	int handleReqfromCtrl ()
	{
		fileLog.writeLog (Log::INFO, string ("handleReqfromCtrl begin"));
		int len;
		int sockclnt;
		if ((len = read (fifo_ctrl_r, &head, headlen)) != headlen) {
			fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl 读取header from Ctrl长度错误 len: ") + to_string (len));
			return -1;
		}

		// 是上传文件请求
		if (head.p == pType::UPLOAD_REQ) {
			if ((len = read (fifo_ctrl_r, &req, head.len)) != head.len) {
				fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl 读取reqBody from Ctrl长度错误 len: ") + to_string (len));
				return -1;
			}
			// need a socket from ctrl!!!!!
			if ((len = read (fifo_ctrl_r, &sockclnt, sizeof (int))) != 4) {
				fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl 读取socket from Ctrl错误 len: ") + to_string (len));
				return -1;
			}
			// 将文件和socket关联添加到fs表
			addfsTable (req.MD5, sockclnt);
			return 0;
			fileLog.writeLog (Log::INFO, string ("handleReqfromCtrl done"));

		}
		else {
			fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl 接收到来自ctrl的错误请求"));
			return -1;
		}
	}

	int getPackageFromClient (socket_t sockclnt)
	{
		int len;
		if ((len = read (fifo_ctrl_r, &head, headlen)) != headlen) {
			fileLog.writeLog (Log::ERROR, string ("uploader 读取header from client长度错误 len: ") + to_string (len));
			return -1;
		}
		if (head.p == pType::UPLOAD_PUSH) {
			if ((len = read (fifo_ctrl_r, &push, head.len)) != head.len) {
				fileLog.writeLog (Log::ERROR, string ("uploader 读取pushBody from client长度错误 len: ") + to_string (len));
				return -1;
			}

		}
	}

	// 是
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
		int cnt = EpollWait ();
		fileLog.writeLog (Log::INFO, string ("epollwait end"));

		for (int i = 0; i < cnt; ++i) {
			auto& ep_ev = this->ep_events[i];
			if (ep_ev.data.fd == fifo_ctrl_r) {
				// 从控制端来的包
				handleReqfromCtrl ();
			}
		}





	}


};


