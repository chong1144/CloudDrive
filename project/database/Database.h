#include "../Package/Package.h"
#include "../Utility/Config.h"
#include "../Utility/Log.h"
#include <iostream>
#include <queue>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <mysql.h>
#include <fstream>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <openssl/md5.h>
#include <string.h>
#include <ctime>

using std::cout;
using std::endl;
using std::queue;
using std::string;

//table names
#define FILES "Files"
#define FILEINDEX "FileIndex"
#define USERS "Users"

// const
#define BUF_SIZE 1024 * 1024
#define MAXEVENTS 5

//config file
#define CONFIG_FILE "../Config.conf"

//log file
#define LOG_FILE "../Log/database.log"


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
    string t;
    tm *now = localtime(NULL);
    t = std::to_string(now->tm_year)+"-"+std::to_string(now->tm_mon)+"-"+std::to_string(now->tm_mday)+" "
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
    string password;
    string userName;
    string hostName;

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
    Database();
    int Run();

    //funtion in one
    int do_mysql_cmd(UniformHeader h);

    string get_pwd_by_uname(string Username);
    int insert_Users(string Username,string Password,string IP);
    int update_Users(string Username,string IP);


    ~Database();
};

//connect database
Database::Database() : config(CONFIG_FILE), log(LOG_FILE)
{
    //init mysql config
    this->hostName = config.getValue("Database", "hostName");
    this->userName = config.getValue("Database", "userName");
    this->databaseName = config.getValue("Database", "DatabaseName");
    this->password = config.getValue("Database", "password");

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
    if (mysql_real_connect(mysql, hostName.c_str(), userName.c_str(), password.c_str(), databaseName.c_str(), 0, NULL, 0) == NULL)
    {
        cout << "mysql_real_connect failed(" << mysql_error(mysql) << ")" << endl;
        exit(-1);
    }
    cout << "mysql connect sucessful" << endl;

    /* 设置字符集，否则读出的字符乱码，即使/etc/my.cnf中设置也不行 */
    mysql_set_character_set(mysql, "gbk");

    //open fifo
    fifo_ctod = open(config.getValue("Global", "fifo_ctod").c_str(), O_RDONLY);
    fifo_dtoc = open(config.getValue("Global", "fifo_dtoc").c_str(), O_WRONLY);
    fifo_rtod = open(config.getValue("Global", "fifo_rtod").c_str(), O_RDONLY);
    fifo_dtor = open(config.getValue("Global", "fifo_dtor").c_str(), O_WRONLY);
    fifo_stod = open(config.getValue("Global", "fifo_stod").c_str(), O_RDONLY);
    fifo_dtos = open(config.getValue("Global", "fifo_dtos").c_str(), O_WRONLY);

    if (fifo_ctod < 0 || fifo_dtoc < 0 || fifo_rtod < 0 || fifo_dtor < 0 || fifo_stod < 0 || fifo_dtor < 0)
    {
        cout << "open fifo error" << endl;
        exit(-1);
    }
}

//disconnect from database
Database::~Database()
{
    mysql_close(mysql);
}

string Database::get_pwd_by_uname(string Username)
{
    string cmd = "select Password from Users where Uname="+generate_string(Username);
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
    }
    
    // 若找不到该用户
    if((row = mysql_fetch_row(result)) == NULL)
    {
        return string("NULL");
    }
        
    else
    {
        return string(row[0]);
    }

}

int Database::insert_Users(string Username,string Password, string IP)
{
    //数据顺序 Uname,Password,IP,Lastlogintime,Createtime
    string cmd = string("insert into Users values(") +generate_string(Username) +","+generate_string(MD5(Password.c_str()))+","+generate_string(IP)+","+generate_timestamp()+","+generate_timestamp() +");";
    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        return 0;
    }
    return 1;

}

int Database::update_Users(string Username,string IP)
{
    string cmd = string("update Users set IP=")+generate_string(IP)+","+"Lastlogintime="+generate_timestamp()+"where Uname="+generate_string(Username) +";";
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
    epoll_event events[MAXEVENTS], ep_ev;
    // listen fifos
    ep_ev.data.fd = fifo_rtod;
    ep_ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, ep_ev.data.fd, &ep_ev);

    ep_ev.data.fd = fifo_stod;
    ep_ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, ep_ev.data.fd, &ep_ev);

    ep_ev.data.fd = fifo_ctod;
    ep_ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, ep_ev.data.fd, &ep_ev);

    UniformHeader header;

    while (1)
    {
        //监听其他进程向其发送命令的管道(阻塞)
        int count = epoll_wait(epfd, events, MAXEVENTS, NULL);
        if (count < 0)
        {
            perror("epoll wait error");
        }
        for (int i = 0; i < count; i++)
        {
            //若该管道有命令写入
            if (events[i].events | EPOLLIN)
            {
                //读出相应的命令头
                int len = recv(events[i].data.fd, &header, sizeof(UniformHeader), MSG_WAITALL);
                if (len <= 0)
                {
                    log.writeLog(Log::ERROR, "recv() error when read header");
                }

                //申请相应大小的空间
                void *temp = new char[header.len];

                //读入命令
                len = recv(events[i].data.fd, temp, header.len, MSG_WAITALL);
                if (len <= 0)
                {
                    log.writeLog(Log::ERROR, "recv() error when rcve body");
                }

                //将其插入命令头队列的队尾
                header_queue.push(header);

                //将命令body部分的指针插入cmd_queue的尾部
                cmd_queue.push(temp);
            }
        }
        //若事件队列不为空
        if (!header_queue.empty())
        {
            //取出事件队列的第一个元素
            header = header_queue.front();

            //清除队首元素
            cmd_queue.pop();

            //执行对应的操作数据库命令
            do_mysql_cmd(header);

            //将命令执行结果传回相应的模块
            //send(result, fifo);
        }
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

        //生成返回包的头
        res_header.len = sizeof(SigninresBody);
        res_header.p = PackageType::SIGNIN_RES;
        res_header_queue.push(res_header);

        //生成返回包的身
        SigninresBody *res = new SigninresBody[1];
        strcpy(res->Session,body->Session);

        //从数据库找该用户
        user_pwd = get_pwd_by_uname(body->Username);

        //用户不存在
        if(user_pwd == "NULL")
        {
            res->code = SIGNIN_UNEXIST_USERNAME;
        }
        //用户存在
        else
        {
            if(MD5(body->Password)!= user_pwd)
            {
                res->code = SIGNIN_INCORRECT_PASSWORD;
            }
            else
            {
                res->code = SIGNIN_SUCCESS;
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
        
        string user_pwd;

        //生成返回包的头
        res_header.len = sizeof(SignupresBody);
        res_header.p = PackageType::SIGNUP_RES;
        res_header_queue.push(res_header);

         //生成返回包的身
        SignupresBody *res = new SignupresBody[1];
        
      
        user_pwd = get_pwd_by_uname(body->Username);

        //未找到该用户
        if(user_pwd == "NULL")
        {
            //为该用户创建新的表项
            insert_Users(body->Username,MD5(body->Password),body->IP);
            res->code = SIGNUP_SUCCESS;
        }
        else
        {
            //告知用户该用户名已存在 
            res->code = SIGNUP_EXISTED_USERNAME;
        }
        
        //将包加入写队列 
        res_queue.push(res);

    }

    //若为文件上传请求 同名文件的覆盖还没做
    else if(h.p == PackageType::UPLOAD_REQ)
    {
        UploadReqBody *body = (UploadReqBody *)cmd_queue.front();
        //查找是否存在相同文件
        cmd = "select * from Files where Hash="+string(body->MD5);
        string fileName = body->fileName;
        bool already_exist = 0;
        char bitmap[65535];
        //init bitmap
        memset(bitmap,sizeof(bitmap),"0");

        //生成返回包的头
        res_header.len = sizeof(UploadRespBody);
        res_header.p = PackageType::UPLOAD_RESP;
        res_header_queue.push(res_header);

         //生成返回包的身
        UploadRespBody *res = new UploadRespBody[1];

       
        if (mysql_query(mysql, cmd.c_str()))
        {
            log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        }
       
        if ((result = mysql_store_result(mysql)) != NULL)
        {
            log.writeLog(Log::ERROR,string("mysql_store_result failed"));
        }
        
        // 若没有找到相同文件
        if((int)mysql_num_rows(result) != 0)
        {
           already_exist = 1;
        }  

        
        //更新file表
        cmd = "insert into Files values(" + \
        string(body->Session)+","+  \
        "\""+string(MD5(body->fileName))+"\""+","+  \
        string(std::to_string(body->fileSize))+","+ \
        "\""+string(body->path)+"\""+","+                \
        generate_timestamp()+","+                   \
        std::to_string(already_exist)+","+"0" +");";
        
        
        if (mysql_query(mysql, cmd.c_str()))
        {
            log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        }

        //编写更新FileIndex表命令
        if(!already_exist){
            cmd = "inster into FileIndex values("+ \
            "\""+string(MD5(body->MD5))+"\""+","+   \
            "0"+","+"\""+string(bitmap)+"\""+");";
        }
        else{
            //更新引用数
            int refcount;
            cmd = "select Refconut from FileIndex where hash="+string(body->MD5);
            if (mysql_query(mysql, cmd.c_str()))
            {
                log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
            }
            if ((result = mysql_store_result(mysql)) == NULL)
            {
                log.writeLog(Log::ERROR,string("mysql_store_result failed"));
            }
            if((row = mysql_fetch_row(result)) != NULL){
                refcount = atoi(row[0]);
                refcount++;
            }

            cmd = string("updata FileIndex set Refcount=")+std::to_string(refcount)+" where hash="+string(body->MD5)+";";

        }

        //更新FileIndex表
        if (mysql_query(mysql, cmd.c_str()))
        {
            log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        }

        if(already_exist)
        {
            res->code = UPLOAD_ALREADY_HAS;
        }
        else{
            res->code = UPLOAD_SUCCESS;
        }
        //将包加入写队列 
        res_queue.push(res);
    }

    //若为文件目录同步请求
    else if(h.p == PackageType::SYN_REQ)
    {
        SYNReqBody *body = (SYNReqBody *)cmd_queue.front(); 
        string cmd = "select Filename,Isdir from Files where Uid="+string(body->Session)+" and Dir="+"\""+string(body->path)+"\""+";";

        //生成返回包的头
        res_header.len = sizeof(SYNRespBody);
        res_header.p = PackageType::UPLOAD_RESP;
        res_header_queue.push(res_header);

         //生成返回包的身
        SYNRespBody *res = new SYNRespBody[1];
        
        //执行查询语句
        if (mysql_query(mysql, cmd.c_str()))
        {
            log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        }

        //获取查询结果
        if ((result = mysql_store_result(mysql)) != NULL)
        {
            log.writeLog(Log::ERROR,string("mysql_store_result failed"));
        }

        res->childNum = (int)mysql_num_rows(result);

        memset(res->ExternInformation,sizeof(res->ExternInformation),0);
        //遍历查找结果
        while ((row = mysql_fetch_row(result)) != NULL)
        {
            //存储结构 [文件/文件夹名,是否为文件夹] 空格 [文件/文件夹名,是否为文件夹]...
           strcat(res->ExternInformation,row[0]);
           strcat(res->ExternInformation,",");
           strcat(res->ExternInformation,row[1]);
           strcat(res->ExternInformation," ");
        }
        res->code = SYN_SUCCESS;

        //将包加入写队列
        res_queue.push(res);
    }

    //若为文件/文件夹拷贝命令
    else if(h.p == PackageType::COPY)
    {
        
    }

    cmd_queue.pop();
}
