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
	// ά����file-socket��
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
	// ��DBģ�鷢�ͻ�ȡ�ļ���Ϣ������
	int sendReqFileInfotoDB (fileHash hashcode)
	{

	};

	void addMap (fileHash hashcode, socket_t sockclnt)
	{
		for (auto sock : fs_map[hashcode]) {
			// �Ѿ���file-socket����
			if (sock.sock == sockclnt)
				return;
		}
		fs_map[hashcode].push_back ({ sockclnt,SOCKETSTATE::IDEL });
	}

	// �ӿ���ģ���ȡ����
	int getReqfromCtrl ()
	{
		int len;
		int sckt;
		if ((len = read (fifo_ctrl_r, &head, headlen)) != headlen) {
			fileLog.writeLog (Log::ERROR, string ("uploader ��ȡheader from Ctrl���ȴ��� len: ") + to_string (len));
			return -1;
		}


		// ���ϴ��ļ�����
		if (head.p == pType::UPLOAD_REQ) {
			if ((len = read (fifo_ctrl_r, &req, head.len)) != head.len) {
				fileLog.writeLog (Log::ERROR, string ("uploader ��ȡreqBody from Ctrl���ȴ��� len: ") + to_string (len));
				return -1;
			}
			// need a socket from ctrl!!!!!
			if ((len = read (fifo_ctrl_r, &sckt, sizeof (int))) != 4) {
				fileLog.writeLog (Log::ERROR, string ("uploader ��ȡsocket from Ctrl���� len: ") + to_string (len));
				return -1;
			}
			addMap (req.MD5, sckt);
			return 0;
		}
		else {
			fileLog.writeLog (Log::ERROR, string ("uploader ���յ�����ctrl�Ĵ�������"));
			return -1;
		}


	}

	int getPackageFromClient (socket_t sockclnt)
	{
		int len;
		if ((len = read (fifo_ctrl_r, &head, headlen)) != headlen) {
			fileLog.writeLog (Log::ERROR, string ("uploader ��ȡheader from client���ȴ��� len: ") + to_string (len));
			return -1;
		}
		if (head.p == pType::UPLOAD_PUSH) {
			if ((len = read (fifo_ctrl_r, &push, head.len)) != head.len) {
				fileLog.writeLog (Log::ERROR, string ("uploader ��ȡpushBody from client���ȴ��� len: ") + to_string (len));
				return -1;
			}

		}
	}

	// ��
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


