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

class Downloader
{


	//database model to/from send model
	int fifo_db_w;
	int fifo_db_r;

	//control model to/from send model
	int fifo_ctrl_w;
	int fifo_ctrl_r;

	//readfile model to/from database model
	int fifo_readfile_w;
	int fifo_readfile_r;

public:
	int sendreqtoDB ();

};