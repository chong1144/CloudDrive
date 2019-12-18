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

	// ��DBģ�鷢�ͻ�ȡ�ļ���Ϣ������
	// �˺��������յ�����CTRL���������ã�head�Լ�uploadreqbody��δ�䣬ֱ��ת������
	int sendReqFileInfotoDB ()
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
	int sendReqSaveFileInfotoDB (const string& filehash)
	{
		int len;
		auto& fileinfo = fileMap.at (filehash);
		strcpy (fileInfoPacket.md5, filehash.data ());
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

	void addfsTable (FileHash hashcode, socket_t sockclnt)
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

	// ������һ����λ�飬�����λ����Ϊsize������û�пտ飬��һ����ȫ�ϴ���ϣ�������
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

	// ����ӿ���ģ��������Ϣ��
	int handleReqfromCtrl ()
	{
		fileLog.writeLog (Log::INFO, string ("handleReqfromCtrl begin"));
		int len;
		int sockclnt;
		// ��ȡheader
		if ((len = read (fifo_ctrl_r, &headPacket, headlen)) != headlen) {
			fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl ��ȡheader from Ctrl���ȴ��� len: ") + to_string (len));
			return -1;
		}

		// ���ϴ��ļ�����
		if (headPacket.p == pType::UPLOAD_REQ) {
			fileLog.writeLog (Log::INFO, string ("handleReqfromCtrl ���� UPLOAD_REQ"));
			if ((len = read (fifo_ctrl_r, &uploadReqPacket, headPacket.len)) != headPacket.len) {
				fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl ��ȡreqBody from Ctrl���ȴ��� len: ") + to_string (len));
				return -1;
			}
			// ���ս��յ��ϴ�����ת����databaseģ��
			sendReqFileInfotoDB ();

			// need a socket from ctrl!!!!!
			if ((len = read (fifo_ctrl_r, &sockclnt, sizeof (int))) != 4) {
				fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl ��ȡsocket from Ctrl���� len: ") + to_string (len));
				return -1;
			}
			// ��socket�Ž��ȴ���ѯ����
			fileLog.writeLog (Log::INFO, string ("handleReqfromCtrl �ȴ���ѯ���������sock: " + to_string (sockclnt)));
			queryQue.push (sockclnt);

			//// ���ļ���socket������ӵ�fs�� ��Ҫ�ȵ���ѯ���ݿ��������

			return 0;
		}
		// ����Ҫ��Ӷ���ָͣ�����Ӧ��ȡ���Ƿ���Ҫ��
		else {
			fileLog.writeLog (Log::ERROR, string ("handleReqfromCtrl ���յ�����ctrl�Ĵ�������"));
			return -1;
		}
	}
	// �����client�������ݰ�
	int handlePacketFromClient (const string& filehash, socket_t sockclnt)
	{
		fileLog.writeLog (Log::INFO, string ("handlePacketFromClient begin"));
		int len;
		if ((len = read (fifo_ctrl_r, &headPacket, headlen)) != headlen) {
			fileLog.writeLog (Log::ERROR, string ("handlePacketFromClient ��ȡheader from client���ȴ��� len: ") + to_string (len));
			return -1;
		}
		if (headPacket.p == pType::UPLOAD_PUSH) {
			if ((len = read (fifo_ctrl_r, &pushPacket, headPacket.len)) != headPacket.len) {
				fileLog.writeLog (Log::ERROR, string ("handlePacketFromClient ��ȡpushBody from client���ȴ��� len: ") + to_string (len));
				return -1;
			}
			// already get right package

			auto& chunk = fileChunk_map[make_pair (filehash, sockclnt)];
			memcpy (chunk.content + chunk.size, pushPacket.content, pushPacket.len);
			chunk.size += pushPacket.len;
			if (pushPacket.last) {
				fileout.WriteFile (chunk);
				// �޸�λʾͼ
				fileMap.at (filehash).chunkBitMap[chunk.chunkNo] = CHUNK_EXIST;
				uint16_t chunkNo = getNextChunkNo (filehash);
				// �Ѿ�û�п��п飬����fetch
				if (chunkNo==fileMap.at(filehash).size) {
					uploadSet.erase (sockclnt);
					idleSet.insert (sockclnt);
					// �ж��ļ��Ƿ�����
					if (isCompleted (filehash)) {
						// �����������ɺ���ɨβ
						
						uploadDone (filehash);
					}
				}
				else {

				}
				// д���ͷ��ļ���
				fileChunk_map.erase (make_pair (filehash, sockclnt));
				fileChunk_map[make_pair (filehash, sockclnt)] = FileChunk{};
			}
		}
		else {
			fileLog.writeLog (Log::ERROR, string ("handlePacketFromClient ���յ�����client�Ĵ�������"));
			return -1;
		}
		fileLog.writeLog (Log::INFO, string ("handlePacketFromClient end"));
		return 0;
	}
	// ��������ݿ�������Ϣ��
	int handlePacketFromDB ()
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
		string filehash (fileInfoPacket.md5, 32);


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

			doneQue.push (queryQue.front ());
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
			sendReqToClient (filehash, chunkNo, queryQue.front ());
			queryQue.pop ();
		}
		else {
			fileLog.writeLog (Log::WARNING, string ("handlePacketFromDB fileInfoδ��������") + to_string (queryQue.front ()));
		}
		// ֻҪ�����ݿⷢ��UPLOAD���󣬾�һ�����ڣ����ؿ��ǲ�����

		fileLog.writeLog (Log::INFO, string ("handlePacketFromDB end"));
		return 0;
	}

	// ��Client����������
	// �޸�socket״̬Ϊupload
	// ����(md5, socket)-->�����ļ���ӳ��
	// ÿ�ֲ����п���socketʱ����
	int sendReqToClient (const string& filehash, uint16_t chunkNo, socket_t sockclnt)
	{
		fileLog.writeLog (Log::INFO, string ("sendReqToClient begin"));
		int len;
		memcpy (this->fetchPacket.MD5, filehash.data (), 32);
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

		// �޸�socket״̬
		if (idleSet.count (sockclnt)) {
			idleSet.erase (sockclnt);
		}
		else {
			fileLog.writeLog (Log::WARNING, string ("sendReqToClient Ŀ��socket��ǰ����idel������"));
		}
		// ��Ϊ���ڴ���״̬
		uploadSet.insert (sockclnt);

		// ����md5��socket�����ļ���ӳ��
		fileChunk_map[make_pair (string (filehash, 32), sockclnt)] = FileChunk{};
		fileLog.writeLog (Log::INFO, string ("sendReqToClient end"));
		return 0;
	}
	
	// ��ͻ��˷�������ź�
	// ����UploadFetchBody��chunk=size
	void sendDoneToClient (const string& filehash)
	{
		auto& fileinfo = fileMap.at (filehash);
		// ����file-socket���е�socket����������ź�
		for (auto& sock : fileinfo.sockSet) {
			sendReqToClient (filehash, fileinfo.size, sock);
		}
	}

	// �ļ�������ɺ��ɨβ
	void uploadDone (const string& filehash)
	{
		sendReqSaveFileInfotoDB (filehash);
		sendDoneToClient (filehash);
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
		// ��ʼ���ӿ���ģ����ź�
		EpollAdd (this->fifo_ctrl_r, EPOLLIN);
		EpollAdd (this->fifo_db_r, EPOLLIN);

		int epcnt;
		while (true) {
			epcnt = EpollWait ();

			for (int i = 0; i < epcnt; ++i) {
				auto& ep_ev = this->ep_events[i];
				// �ӿ��ƶ����İ�
				if (ep_ev.data.fd == fifo_ctrl_r) {
					if (ep_ev.events & EPOLLERR) {
						// ��μ���fd��Ӧ��socket����ϸ��Ϣ������
						fileLog.writeLog (Log::ERROR, string ("EPOLLERR fifo_ctrl_r ") + " EpollDel!!!");
						EpollDel (ep_ev.data.fd);
					}
					if (ep_ev.events & EPOLLIN) {
						handleReqfromCtrl ();
					}
				}
				// �����ݿ�ģ�����İ� 
				if (ep_ev.data.fd == fifo_db_r) {
					if (ep_ev.events & EPOLLIN) {
						handlePacketFromDB ();
					}
				}
				// ��client�����İ�
				for (auto& file : fileMap) {
					for (auto& sock : file.second.sockSet) {
						if (sock == ep_ev.data.fd) {
							// ������ʱ�Ĵ�����ɾ���������
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


