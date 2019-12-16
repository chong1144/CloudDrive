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
#include <sys/epoll.h>
#include <openssl/md5.h>
#include <string.h>
#include <ctime>
#include <map>
#include <mysql/mysql.h>
#include <unistd.h>
#include "../Utility/Log.h"
using std::to_string;
using size_t = uint64_t;
using pType = PackageType;
const size_t headlen = sizeof (UniformHeader);
using std::queue;
const int MAXEVENTS = 20;



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
	int fifo_readfile_w;
	int fifo_readfile_r;


	UniformHeader head;
	UploadReqBody req;
	UploadRespBody resp;
	UploadFetchBody fetch;
	UploadPushBody push;
	Log fileLog;
public:
	// 向DB模块发送获取文件信息的请求
	int sendReqFileInfotoDB (fileHash hashcode)
	{

	};

	void addMap (fileHash hashcode, socket_t sockclnt)
	{
		for (auto sock : fs_map[hashcode]) {
			// 已经在file-socket表中
			if (sock.sock == sockclnt)
				return;
		}
		fs_map[hashcode].push_back ({ sockclnt,SOCKETSTATE::IDEL });
	}

	// 从控制模块获取命令
	int getReqfromCtrl ()
	{
		int len;
		int sckt;
		if ((len = read (fifo_ctrl_r, &head, headlen)) != headlen) {
			fileLog.writeLog (Log::ERROR, string ("uploader 读取header from Ctrl长度错误 len: ") + to_string (len));
			return -1;
		}


		// 是上传文件请求
		if (head.p == pType::UPLOAD_REQ) {
			if ((len = read (fifo_ctrl_r, &req, head.len)) != head.len) {
				fileLog.writeLog (Log::ERROR, string ("uploader 读取reqBody from Ctrl长度错误 len: ") + to_string (len));
				return -1;
			}
			// need a socket from ctrl!!!!!
			if ((len = read (fifo_ctrl_r, &sckt, sizeof (int))) != 4) {
				fileLog.writeLog (Log::ERROR, string ("uploader 读取socket from Ctrl错误 len: ") + to_string (len));
				return -1;
			}
			addMap (req.MD5, sckt);
			return 0;
		}
		else {
			fileLog.writeLog (Log::ERROR, string ("uploader 接收到来自ctrl的错误请求"));
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




	}


};


