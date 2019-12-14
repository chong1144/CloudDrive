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

string generate_string(string src)
{
    
    string t = "\"";
    t += src;
    t += "\"";
    return t;
    
}

string generate_timestamp()
{
    //生成时间戳
    time_t tmNow = time(NULL);
    string t;
    tm *now = localtime(&tmNow);
    t = std::to_string(now->tm_year+1900)+"-"+std::to_string(now->tm_mon+1)+"-"+std::to_string(now->tm_mday)+" "
    +std::to_string(now->tm_hour)+":"+std::to_string(now->tm_min)+":"+std::to_string(now->tm_sec);

    return generate_string(t);
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

//connect database
Database::Database(string config_file,string log_file) : config(config_file), log(log_file)
{
    //init mysql config
    this->mysqlHostName = config.getValue("Database", "hostName");
    this->mysqlUserName = config.getValue("Database", "userName");
    this->databaseName = config.getValue("Database", "databaseName");
    this->mysqlPassword = config.getValue("Database", "password");

    //connect mysql database
    /* 初始化 mysql 变量，失败返回NULL */
    if ((mysql = mysql_init(NULL)) == NULL)
    {
        cout << "mysql_init failed" << endl;
        exit(-1);
    }

    /* 连接数据库，失败返回NULL
 *        1、mysqld没运行
 *               2、没有指定名称的数据库存在 */
    if (mysql_real_connect(mysql, mysqlHostName.c_str(), mysqlUserName.c_str(), mysqlPassword.c_str(), databaseName.c_str(), 0, NULL, 0) == NULL)
    {
        cout << "mysql_real_connect failed(" << mysql_error(mysql) << ")" << endl;
        exit(-1);
    }
    //cout << "mysql connect sucessful" << endl;
    log.writeLog(Log::INFO,"mysql connect sucessful");

    /* 设置字符集，否则读出的字符乱码，即使/etc/my.cnf中设置也不行 */
    mysql_set_character_set(mysql, "gbk");

    //open fifo
    fifo_ctod = open(config.getValue("Global", "path_fifo_ctod").c_str(), O_RDONLY);
    fifo_dtoc = open(config.getValue("Global", "path_fifo_dtoc").c_str(), O_WRONLY);
    fifo_rtod = open(config.getValue("Global", "path_fifo_rtod").c_str(), O_RDONLY);
    fifo_dtor = open(config.getValue("Global", "path_fifo_dtor").c_str(), O_WRONLY);
    fifo_stod = open(config.getValue("Global", "path_fifo_stod").c_str(), O_RDONLY);
    fifo_dtos = open(config.getValue("Global", "path_fifo_dtos").c_str(), O_WRONLY);
     
    if (fifo_ctod < 0 || fifo_dtoc < 0 || fifo_rtod < 0 || fifo_dtor < 0 || fifo_stod < 0 || fifo_dtor < 0)
    {
        cout << "open fifo error" << endl;
        exit(-1);
    }
    //cout << "open fifo success"<<endl;
    log.writeLog(Log::INFO,"open fifo success");
}

//disconnect from database
Database::~Database()
{
    mysql_close(mysql);
}
/***********************************
 * 
 * functions for table Users
 * 
 * 
 * ********************************/

pair<string,string> Database::get_uid_pwd_by_uname(string Username)
{
    pair<string,string> temp;
    string cmd = "select Uid,Password from Users where Uname="+generate_string(Username)+";";
    //cout << cmd <<endl;
    /* 进行查询，成功返回0，不成功非0
         1、查询字符串存在语法错误
                2、查询不存在的数据表 */
    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
    }
    
        /* 将查询结果存储起来，出现错误则返回NULL
        注意：查询结果为NULL，不会返回NULL */
    if ((result = mysql_store_result(mysql)) == NULL)
    {
        log.writeLog(Log::ERROR,string("mysql_store_result failed"));
        temp.first = "NULL";
        temp.second = "NULL";
        return temp;
    }

    //检测返回结果数
    if(mysql_num_rows(result) == 0)
    {
        temp.first = string("NULL");

        temp.second = string("NULL");
        return temp;
    }

    
    row = mysql_fetch_row(result);
    temp.first = string(row[0]);
    temp.second = string(row[1]);
    return temp;
    
}

int Database::Users_Insert(string Username,string Password, string IP)
{
    //数据顺序 Uname,Password,IP,Lastlogintime,Createtime
    string cmd = string("insert into Users values(")+"NULL," +generate_string(Username) +","+generate_string(MD5(Password.c_str()))+","+generate_string(IP)+","+generate_timestamp()+","+generate_timestamp() +");";
    //cout <<cmd<<endl;
    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        return 0;
    }
    return 1;

}

int Database::Users_Update(string Username,string IP)
{
    string cmd = string("update Users set IP=")+generate_string(IP)+","+"Lastlogintime="+generate_timestamp()+"where Uname="+generate_string(Username) +";";
    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        return 0;
    }
    return 1;
}

/***********************************
 * 
 * functions for table Files
 * 
 * 
 * ********************************/

bool Database::Files_Isdir(string Uid,string Filename,string path)
{
    string cmd = "select Isdir from Files where Uid="+ \
    generate_string(Uid)+" and "+\
    "Filename="+generate_string(Filename)+" and "+\
    "Path="+generate_string(path)+";";

    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
    }

     if ((result = mysql_store_result(mysql)) == NULL)
    {
        log.writeLog(Log::ERROR,string("mysql_store_result failed"));
    }
    
    // 若找不到文件
    if((row = mysql_fetch_row(result)) == NULL)
    {
        return false;
    }
    else
    {
        return atoi(row[0]) ;
    }
}

string Database::Files_Get_Hash(string Uid,string Filename,string path)
{
    string cmd = "select Hash from Files where Uid="+ \
    generate_string(Uid)+" and "+\
    "Filename="+generate_string(Filename)+" and "+\
    "Path="+generate_string(path)+";";
    
    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
    }

     if ((result = mysql_store_result(mysql)) == NULL)
    {
        log.writeLog(Log::ERROR,string("mysql_store_result failed"));
    }
    
    // 若找不到文件
    if((row = mysql_fetch_row(result)) == NULL)
    {
        return "NULL";
    }
    else
    {
        return string(row[0]);
    }
}

int Database::Files_Insert(string Uid,string Filename,string path,string hash,bool Isdir)
{
    string cmd = "insert into Files values(" + \
        string(Uid)+","+  \
        generate_string(Filename)+","+  \
        generate_string(path)+","+                \
        generate_string(hash)+","+  \
        generate_timestamp()+","+
        to_string(Isdir) +");";
        
    //cout << cmd <<endl;
    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        return 0;
    }
    return 1;
}

int Database::Files_Delete(string Uid,string Filename,string path)
{
    string cmd = "delete  from Files where Uid="+ \
    generate_string(Uid)+" and "+\
    "Filename="+generate_string(Filename)+" and "+\
    "Path="+generate_string(path)+";";

    FileIndex_Refdec(Files_Get_Hash(Uid,Filename,path));

    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        return 0;
    }

    
    return 1;

}

int Database::Files_Copy(string Uid,string Filename,string path,string FilenameTo ,string pathTo)
{
    string hash;

    hash = Files_Get_Hash(Uid,Filename,path);
    bool isDir = Files_Isdir(Uid,Filename,path);

    FileIndex_Refinc(hash);

    return Files_Insert(Uid,FilenameTo,pathTo,hash,isDir);

}

int Database::Files_Move(string Uid,string Filename,string path,string FilenameTo ,string pathTo)
{
    string cmd = string("update Files set ")
    +" Filename="+generate_string(FilenameTo)+","\
    + "Path="+generate_string(pathTo)
    +" where "+ \
    "Uid="+generate_string(Uid)+" and "+\
    "Filename="+generate_string(Filename)+" and "+\
    "Path="+generate_string(path)+";";

    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        return 0;
    }
    return 1;

    
}

vector<pair<string,string>>Database::get_child_files(string Uid,string path)
{
    string cmd = string("select Filename,Isdir from Files where ")+
    " Uid="+generate_string(Uid)+ " and "\
    " Path="+generate_string(path)+";";

    vector<pair<string,string>> v;

    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
    }
        /* 将查询结果存储起来，出现错误则返回NULL
        注意：查询结果为NULL，不会返回NULL */
    if ((result = mysql_store_result(mysql)) == NULL)
    {
        log.writeLog(Log::ERROR,string("mysql_store_result failed"));
    }
    
    // 若找不到该用户
    while((row = mysql_fetch_row(result)) != NULL)
    {
        v.push_back(std::make_pair(string(row[0]),string(row[1])));
    }

    return v;
}

/***********************************
 * 
 * functions for table FileIndex
 * 
 * 
 * ********************************/

bool Database::file_exist(string hashCode)
{
    string cmd = "select * from FileIndex where Hash="+generate_string(hashCode)+";";
   // cout<<cmd<<endl;
    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
    }
    
    if ((result = mysql_store_result(mysql)) == NULL)
    {
        log.writeLog(Log::ERROR,string("mysql_store_result failed"));
    }
    
    // 若没有找到相同文件
    if((row = mysql_fetch_row(result)) == NULL)
    {
        return false;
    }  
    return true;
} 

int Database::FileIndex_Insert(string hash,int size)
{
    char bitmap[BITMAP_SIZE] ;
    memset(bitmap,0,sizeof(bitmap));

    memset(bitmap,'0',size/1024+1 - (size%1024==0));
    
    string cmd = "insert into FileIndex values("+ \
    generate_string(hash)+","+   \
    to_string(size)+","+\
    "1"+","+    \
    generate_string(bitmap)+","+
    "0"+");";

    //cout <<cmd <<endl;
    //更新FileIndex表
    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        return 0;
    }
    return 1;
}

int Database::FileIndex_Delete(string hash)
{
    string cmd = string("delete from FileIndex where ")
    + " Hash="+generate_string(hash);

    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
    }

}

pair<int,int> Database::FileIndex_Get_Ref_Complete(string hash)
{
    string cmd = string("select Refcount,Complete from FileIndex where ")+ \
    "Hash="+generate_string(hash)+";";

    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
    }

    if ((result = mysql_store_result(mysql)) == NULL)
    {
        log.writeLog(Log::ERROR,string("mysql_store_result failed"));
    }
    
    // 若找不到该用户
    if((row = mysql_fetch_row(result)) == NULL)
    {
        return std::make_pair(0,0);
    }
    else
    {
        return std::make_pair(atoi(row[0]),atoi(row[1]));
    }

}

int Database::FileIndex_Refinc(string hash)
{
    int Refcount = FileIndex_Get_Ref_Complete(hash).first;
    Refcount ++;
    return FileIndex_UpdateRef(hash,Refcount);
}

int Database::FileIndex_Refdec(string hash)
{
    int Refcount = FileIndex_Get_Ref_Complete(hash).first;
    Refcount--;

    if(Refcount <= 0)
    {
        return FileIndex_Delete(hash);
    }
    else{
        return FileIndex_UpdateRef(hash,Refcount);
    }
}

int Database::FileIndex_UpdateRef(string hash,int Refcount)
{
    string cmd=string("update FileIndex set ")+ \
    " Refcount= "+to_string(Refcount)+\
    " where "+\
    " Hash= "+generate_string(hash)+";";

     if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        return 0;
    }
    return 1;
}

string Database::FileIndex_GetBitmap(string hash)
{
    //char Bitmap[BITMAP_SIZE];
    string Bitmap;

    string cmd = string("select Bitmap from FileIndex where ")+ \
    "Hash = "+generate_string(hash) + ";";

     if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
    }

    if ((result = mysql_store_result(mysql)) == NULL)
    {
        log.writeLog(Log::ERROR,string("mysql_store_result failed"));
    }
    
    // 若找不到该用户
    if((row = mysql_fetch_row(result)) == NULL)
    {
        return "NULL";
    }
    else
    {
       Bitmap += row[0];
        return Bitmap;
    }
    
    

}

int Database::FileIndex_UpdataBitmap(string hash,string Bitmap)
{
     string cmd=string("update FileIndex set ")+ \
    " Bitmap= "+generate_string(Bitmap)+\
    " where "+\
    " Hash= "+generate_string(hash)+";";

     if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        return 0;
    }
    return 1;
}

int Database::Run()
{
    //init epoll
    int epfd = epoll_create1(0);
    if(epfd==-1)
    {
        perror ("epoll_create1()");
		exit (-1);
    }
    epoll_event events[MAXEVENTS], ep_ev;
    // listen fifos
    ep_ev.data.fd = fifo_ctod;
    ep_ev.events = EPOLLIN | EPOLLERR;
    epoll_ctl(epfd, EPOLL_CTL_ADD, ep_ev.data.fd, &ep_ev);

    ep_ev.data.fd = fifo_rtod;
    ep_ev.events = EPOLLIN | EPOLLERR;
    epoll_ctl(epfd, EPOLL_CTL_ADD, ep_ev.data.fd, &ep_ev);

    ep_ev.data.fd = fifo_stod;
    ep_ev.events = EPOLLIN| EPOLLERR;
    epoll_ctl(epfd, EPOLL_CTL_ADD, ep_ev.data.fd, &ep_ev);

    

    //cout << "epoll events create success" <<endl;
    log.writeLog(Log::INFO,"epoll events create success");

    UniformHeader header;

    while (1)
    {
        //监听其他进程向其发送命令的管道(阻塞)
        //cout<<"epoll waiting ..."<<endl;
        int count = epoll_wait(epfd, events, MAXEVENTS, -1);
        if (count == -1)
        {
            perror("epoll wait error");
            exit(-1);
        }
        //cout << "epoll count:" <<count <<endl;
        for (int i = 0; i < count; i++)
        {

            //若该管道有命令写入
            if (events[i].events | EPOLLIN)
            {
                //读出相应的命令头
                int len = read(events[i].data.fd, &header, sizeof(UniformHeader));
                if (len <= 0)
                {
                    //对端连接断开
                    log.writeLog(Log::ERROR, string("fifo closed ")+to_string(events[i].data.fd));
                    exit(-1);
                }
                //cout << header.p << " "<<header.len<<endl;

                //申请相应大小的空间
                void *temp = new char[header.len];

                //读入命令
                len = read(events[i].data.fd, temp, header.len);
                if (len <= 0)
                {
                    //对端连接断开
                    log.writeLog(Log::ERROR, string("fifo closed ")+to_string(events[i].data.fd));
                    exit(-1);
                }

                //将其插入命令头队列的队尾
                header_queue.push(header);

                //将命令body部分的指针插入cmd_queue的尾部
                cmd_queue.push(temp);
            }
        }
        //若事件队列不为空
        while (!header_queue.empty())
        {
            //取出事件队列的第一个元素
            header = header_queue.front();

            //清除队首元素
            header_queue.pop();

            //执行对应的操作数据库命令
            do_mysql_cmd(header);

        
        }
       
        //若命令执行结果队列不为空
        while(!res_header_queue.empty())
        {
            
            //取出结果队列第一个元素
            header = res_header_queue.front();
            //cout << header.p<<" "<<header.len <<endl;
            //清除队首元素
            res_header_queue.pop();

            
            //向相应的管道发送结果
            send_back(header);
            

        }
        //getchar();
    }
    
}

int Database::do_mysql_cmd(UniformHeader h)
{
    string cmd;
    UniformHeader res_header;
    
    //若为登陆事件
    if (h.p == PackageType::SIGNIN)
    {
        //get cmd body
        SigninBody *body = (SigninBody *)cmd_queue.front();
        string user_pwd;
        pair <string,string> uid_pwd;

        //cout<<body->Password<<endl;
        log.writeLog(Log::INFO, "[Login] IP:"+string(body->IP)+" Username:"+string(body->Username));


        //生成返回包的头
        res_header.len = sizeof(SigninresBody);
        res_header.p = PackageType::SIGNIN_RES;
        res_header_queue.push(res_header);

        //生成返回包的身
        SigninresBody *res = new SigninresBody[1];
        

        //从数据库找该用户
        uid_pwd = get_uid_pwd_by_uname(body->Username);
        user_pwd = uid_pwd.second;
        strcpy(res->Session,"");

        //用户不存在
        if(user_pwd == "NULL")
        {
            res->code = SIGNIN_UNEXIST_USERNAME;
            log.writeLog(Log::INFO,"[Login Failed] UNEXIST_USERNAME");
        }
        //用户存在
        else
        {
            if(MD5(body->Password)!= user_pwd)
            {
                //cout << MD5(body->Password)<<endl;
                res->code = SIGNIN_INCORRECT_PASSWORD;
                log.writeLog(Log::INFO,"[Login Failed] INCORRECT_PASSWORD");
            }
            else
            {
                //登陆成功 更新数据
                strcpy(res->Session,uid_pwd.first.c_str());
                res->code = SIGNIN_SUCCESS;
                Users_Update(body->Username,body->IP);
                //log.writeLog(Log::INFO,string("update user ")+body->Username+" success");
                log.writeLog(Log::INFO,"[Login Success]");
            }
        }
        
       
        //将包加入写队列 
        res_queue.push(res);
        

        //释放内存
        delete body;
    }

    //若为注册事件
    else if (h.p == PackageType::SIGNUP)
    {
        SignupBody *body = (SignupBody *)cmd_queue.front();
        
        pair <string,string> uid_pwd;

        //注册日志
        log.writeLog(Log::INFO, "[Register] IP:"+string(body->IP)+" Username:"+string(body->Username));

        //生成返回包的头
        res_header.len = sizeof(SignupresBody);
        res_header.p = PackageType::SIGNUP_RES;
        res_header_queue.push(res_header);

        
        
         //生成返回包的身
        SignupresBody *res = new SignupresBody[1];
        

        uid_pwd = get_uid_pwd_by_uname(body->Username);

        //未找到该用户
        if(uid_pwd.second == "NULL")
        {
            //为该用户创建新的表项
            Users_Insert(body->Username,body->Password,body->IP);
            res->code = SIGNUP_SUCCESS;

            uid_pwd = get_uid_pwd_by_uname(body->Username);
            //cout<<uid_pwd.first<<" "<<uid_pwd.second<<endl;
            //给session赋值
            strcpy(res->Session,uid_pwd.first.c_str());

            log.writeLog(Log::INFO,"[Register Success]");
        }
        else
        {
            //告知用户该用户名已存在 

            res->code = SIGNUP_EXISTED_USERNAME;
            strcpy(res->Session,"");
            log.writeLog(Log::INFO,"[Register Failed]");
        }
        
        //将包加入写队列 
        res_queue.push(res);

       
        //释放内存
        
        delete body;
        
    }

    //若为文件上传请求 同名文件的覆盖还没做
    else if(h.p == PackageType::UPLOAD_REQ)
    {
        UploadReqBody *body = (UploadReqBody *)cmd_queue.front();

        //生成返回包的头
        res_header.len = sizeof(UploadRespBody);
        res_header.p = PackageType::UPLOAD_RESP;
        res_header_queue.push(res_header);

        log.writeLog(Log::INFO, "[Upload Req] Uid:"+string(body->Session)+", FileName:"+string(body->fileName)+", Filesize:"+to_string(body->fileSize)+", Path:"+string(body->path)+", Isdir:"+to_string(body->isDir));


         //生成返回包的身
        UploadRespBody *res = new UploadRespBody[1];
        strcpy(res->Session,body->Session);

        //若文件已存在
        if(file_exist(body->MD5))
        {
            FileIndex_Refinc(body->MD5);
            res->code = UPLOAD_ALREADY_HAS;
            log.writeLog(Log::INFO,"[Upload Req Success] ALREADY_HAS");
        } 
        else
        {
            if(!body->isDir)
                FileIndex_Insert(body->MD5,body->fileSize);
            res->code = UPLOAD_SUCCESS;
            log.writeLog(Log::INFO,"[Upload Req Success]");
        }
        Files_Insert(body->Session,body->fileName,body->path,body->MD5,body->isDir);

    
        //将包加入写队列 
        res_queue.push(res);

        delete body;
    }

    //若为文件目录同步请求
    else if(h.p == PackageType::SYN_REQ)
    {
        SYNReqBody *body = (SYNReqBody *)cmd_queue.front(); 
        //生成返回包的头
        res_header.len = sizeof(SYNRespBody);
        res_header.p = PackageType::SYN_RESP;
        res_header_queue.push(res_header);

         //生成返回包的身
        SYNRespBody *res = new SYNRespBody[1];
        strcpy(res->Session,body->Session);

        log.writeLog(Log::INFO, "[SYN Req] Uid:"+string(body->Session)+", path:"+string(body->path));

        //获取路径下的子文件和文件夹 结果放在ExternInformation中，存放格式为： 文件/文件夹名,是否为文件夹 空格 文件/文件夹名,是否为文件夹 .....
        vector<pair<string,string>> children;
        
        children = get_child_files(body->Session,body->path);

        res->childNum = children.size();
        string temp="";
        pair<string,string> filename_isdir;

        while(!children.empty())
        {
            filename_isdir = children.back();
            children.pop_back();

            temp += filename_isdir.first+","+filename_isdir.second+" ";
        }
        strcpy(res->ExternInformation,temp.c_str());
        res->code = SYN_SUCCESS;
        log.writeLog(Log::INFO,"[SYN Req Success]");
        //将包加入写队列
        res_queue.push(res);
    }

    //若为文件/文件夹拷贝命令
    else if(h.p == PackageType::COPY)
    {
        CopyBody *body = (CopyBody *)cmd_queue.front();

        //生成返回包的头
        res_header.len = sizeof(CopyRespBody);
        res_header.p = PackageType::COPY_RES;
        res_header_queue.push(res_header);

         //生成返回包的身
        CopyRespBody *res = new CopyRespBody[1];
        strcpy(res->Session,body->Session);

        log.writeLog(Log::INFO, "[COPY Req] Uid:"+string(body->Session)+", filename:"+string(body->fileName)+", path:"+string(body->path)+", filenameTo:"+string(body->fileNameTo)+", pathTo:"+string(body->pathTo));


        if(Files_Copy(body->Session,body->fileName,body->path,body->fileNameTo,body->pathTo))
        {
            res->code = COPY_SUCCESS;
            log.writeLog(Log::INFO,"[COPY Success]");
        }
        else
        {
            res->code = COPY_FAILED;
            log.writeLog(Log::WARNING,"[COPY Failed]");
        }
        
        //将包加入写队列 
        res_queue.push(res);

        delete body;
    }
    //若为文件移动
    else if(h.p == PackageType::MOVE)
    {
        MoveBody *body = (MoveBody *)cmd_queue.front();

        //生成返回包的头
        res_header.len = sizeof(MoveRespBody);
        res_header.p = PackageType::MOVE_RES;
        res_header_queue.push(res_header);



         //生成返回包的身
        MoveRespBody *res = new MoveRespBody[1];
        strcpy(res->Session,body->Session);

        log.writeLog(Log::INFO, "[MOVE Req] Uid:"+string(body->Session)+", filename:"+string(body->fileName)+", path:"+string(body->path)+", filenameTo:"+string(body->fileNameTo)+", pathTo:"+string(body->pathTo));


        if(Files_Move(body->Session,body->fileName,body->path,body->fileNameTo,body->pathTo))
        {
            res->code = MOVE_SUCCESS;
            log.writeLog(Log::INFO, "[MOVE Success]");
        }
        else
        {
            res->code = MOVE_FAILED;
            log.writeLog(Log::WARNING, "[MOVE Failed]");
        }
        
        //将包加入写队列 
        res_queue.push(res);

        delete body;
    }
    //若为文件删除
    else if(h.p == PackageType::DELETE)
    {
        DeleteBody *body = (DeleteBody *)cmd_queue.front();

        //生成返回包的头
        res_header.len = sizeof(DeleteRespBody);
        res_header.p = PackageType::DELETE_RES;
        res_header_queue.push(res_header);

         //生成返回包的身
        DeleteRespBody *res = new DeleteRespBody[1];
        strcpy(res->Session,body->Session);

        log.writeLog(Log::INFO, "[DELETE Req] Uid:"+string(body->Session)+", filename:"+string(body->fileName)+", path:"+string(body->path));


        if(Files_Delete(body->Session,body->fileName,body->path))
        {
            
            res->code = DELETE_SUCCESS;
            log.writeLog(Log::INFO,"[DELETE Success]");
        }
        else
        {
            res->code = DELETE_FAILED;
            log.writeLog(Log::WARNING,"[DELETE Failed]");
        }
                
        

        
        //将包加入写队列 
        res_queue.push(res);

        delete body;
    }

    cmd_queue.pop();
    return 1;
}


int Database::send_back(UniformHeader header)
{
    if(header.p == PackageType::COPY_RES)
    {
        CopyRespBody *body = (CopyRespBody*)res_queue.front();
        write(fifo_dtoc,&header,sizeof(header));
        write(fifo_dtoc,body,sizeof(CopyRespBody));
        delete body;
    }
    else if(header.p == PackageType::MOVE_RES)
    {
        MoveRespBody *body = (MoveRespBody*)res_queue.front();
        write(fifo_dtoc,&header,sizeof(header));
        write(fifo_dtoc,body,sizeof(MoveRespBody));
        delete body;
    }
    else if(header.p == PackageType::DELETE_RES)
    {
        DeleteRespBody *body = (DeleteRespBody*)res_queue.front();
        write(fifo_dtoc,&header,sizeof(header));
        write(fifo_dtoc,body,sizeof(DeleteRespBody));
        delete body;
    }
    else if(header.p == PackageType::SIGNIN_RES)
    {
        SigninresBody *body = (SigninresBody*)res_queue.front();
        write(fifo_dtoc,&header,sizeof(header));
        write(fifo_dtoc,body,sizeof(SigninresBody));
        delete body;
    }
    else if(header.p == PackageType::SIGNUP_RES)
    {
        SignupresBody *body = (SignupresBody*)res_queue.front();
        write(fifo_dtoc,&header,sizeof(header));
        write(fifo_dtoc,body,sizeof(SignupresBody));
        delete body;
    }
    else if (header.p == PackageType::SYN_RESP)
    {
        SYNReqBody *body = (SYNReqBody*)res_queue.front();
        write(fifo_dtoc,&header,sizeof(header));
        write(fifo_dtoc,body,sizeof(SYNReqBody));
        delete body;
    }
    else if (header.p == PackageType::UPLOAD_RESP)
    {
        UploadRespBody *body = (UploadRespBody*)res_queue.front();
        write(fifo_dtoc,&header,sizeof(header));
        write(fifo_dtoc,body,sizeof(UploadRespBody));
        delete body;
    }
    
    

    
    res_queue.pop();
}
