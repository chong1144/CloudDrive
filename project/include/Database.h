#include "Package/Package.h"
#include "Config.h"
#include "Log.h"
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
#include<mysql/mysql.h>
#include <unistd.h>

using std::cout;
using std::endl;
using std::queue;
using std::string;
using std::pair;
using std::to_string;
using std::vector;
using std::map;

//table names
#define FILES "Files"
#define FILEINDEX "FileIndex"
#define USERS "Users"

// const
#define BUF_SIZE 1024 * 1024
#define MAXEVENTS 5
#define BITMAP_SIZE 65535





string MD5(const char *data);
string generate_string(string src);
string generate_timestamp();

//utility
string MD5(const char * data)
{
	MD5_CTX ctx;
    string res;
	unsigned char md[16];
	char tmp[3]={'\0'};
	int i;
	MD5_Init(&ctx);
	MD5_Update(&ctx,data,strlen(data));
	MD5_Final(md,&ctx);

	for( i=0; i<16; i++ ){
		sprintf(tmp,"%02x",md[i]);
		//strcat(buf,tmp);
        res += string(tmp);
	}
	return res;
}





class Database
{
private:
    //buffer

    //mysql object
    MYSQL *mysql;
    MYSQL_RES *result;
    MYSQL_ROW row;

    //mysql configs
    string databaseName;
    string mysqlPassword;
    string mysqlUserName;
    string mysqlHostName;

    //receive model to/from database model
    int fifo_rtod;
    int fifo_dtor;

    //send model to/from database model
    int fifo_stod;
    int fifo_dtos;

    //control model to/from database model
    int fifo_ctod;
    int fifo_dtoc;

    //event queue
    queue<UniformHeader> header_queue;
    queue<void *> cmd_queue;

    //response queue
    queue<UniformHeader> res_header_queue;
    queue<void *> res_queue;
    //config
    Config config;

    //log
    Log log;

public:
    Database(string config_file,string log_file);
    int Run();

    //funtion in one
    int do_mysql_cmd(UniformHeader h);
    int send_back(UniformHeader header);

    pair<string,string> get_uid_pwd_by_uname(string Username);
    int Users_Insert(string Username,string Password,string IP);
    int Users_Update(string Username,string IP);


    
    bool Files_Isdir(string Uid,string Filename,string path );
    string Files_Get_Hash(string Uid,string Filename,string path);
    int Files_Insert(string Uid,string Filename,string path,string hash,bool Isdir);
    int Files_Move(string Uid,string Filename,string path,string FilenameTo ,string pathTo);
    int Files_Copy(string Uid,string Filename,string path,string FilenameTo ,string pathTo);
    int Files_Delete(string Uid,string Filename,string path);
    vector<pair<string,string>>get_child_files(string Uid,string path);

    int FileIndex_Insert(string hash,int size);
    bool file_exist(string hashCode);
    int FileIndex_Delete(string hash);
    pair<int,int> FileIndex_Get_Ref_Complete(string hash);
    int FileIndex_Refinc(string hash);
    int FileIndex_Refdec(string hash);
    string FileIndex_GetBitmap(string hash);
    int FileIndex_UpdateRef(string hash,int Refcount);
    int FileIndex_UpdataBitmap(string hash,string Bitmap);

    ~Database();
};