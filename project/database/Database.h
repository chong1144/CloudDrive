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

using std::queue;
using std::string;
using std::cout;
using std::endl;

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

/*
database events:

User events
1. add new user
2. delete user
3. modify user info 
4. authenticate user

File events
5. add new file
6. add new dir
7. move file
8. move dir
9. copy file
10. copy dir
11. delete file
12. delete dir
13. reqest file structure

*/
enum DatabaseEvent
{
    ADD_USER,
    DELETE_USER,
    MODIFY_USER,
    AUTHENTICATE_USER,
    ADD_FILE,
    ADD_DIR,
    MOVE_FILE,
    MOVE_DIR,
    COPY_FILE,
    COPY_DIR,
    DELETE_FILE,
    DELETE_DIR,
    REQ_FILE_STRUCTURE
};

class Database
{
private:
    //buffer
    char buffer[BUF_SIZE];

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
    queue<UniformHeader> cmd_queue;

    //config
    Config config;

    //log
    Log log;

    // database function
    int create_user();
    int delete_user();
    int modify_user();
    int authenticate_user();
    int add_file();
    int add_dir();
    int move_file();
    int move_dir();
    int delete_file();
    int delete_dir();
    int file_structure_req();

public:
    Database();
    int Run();
    ~Database();
};

//connect database
Database::Database() : config(CONFIG_FILE),log(LOG_FILE)
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
    fifo_ctod = open(config.getValue("Global","fifo_ctod").c_str(), O_RDONLY);
    fifo_dtoc = open(config.getValue("Global","fifo_dtoc").c_str(), O_WRONLY);
    fifo_rtod = open(config.getValue("Global","fifo_rtod").c_str(), O_RDONLY);
    fifo_dtor = open(config.getValue("Global","fifo_dtor").c_str(), O_WRONLY);
    fifo_stod = open(config.getValue("Global","fifo_stod").c_str(), O_RDONLY);
    fifo_dtos = open(config.getValue("Global","fifo_dtos").c_str(), O_WRONLY);

    if(fifo_ctod < 0 || fifo_dtoc<0 || fifo_rtod < 0 ||fifo_dtor <0 || fifo_stod<0||fifo_dtor<0){
        cout<< "open fifo error"<<endl;
        exit(-1);
    }

}

//disconnect from database
Database::~Database()
{
    mysql_close(mysql);
}

int Database::Run()
{
    //init epoll
    int epfd = epoll_create1(0);
    epoll_event events[MAXEVENTS], ep_ev;
    // listen fifos
    ep_ev.data.fd = fifo_rtod;
    ep_ev.events = EPOLLIN;
    epoll_ctl(epfd,EPOLL_CTL_ADD,ep_ev.data.fd,&ep_ev);

    ep_ev.data.fd = fifo_stod;
    ep_ev.events = EPOLLIN;
    epoll_ctl(epfd,EPOLL_CTL_ADD,ep_ev.data.fd,&ep_ev);

    ep_ev.data.fd = fifo_ctod;
    ep_ev.events = EPOLLIN;
    epoll_ctl(epfd,EPOLL_CTL_ADD,ep_ev.data.fd,&ep_ev);

    UniformHeader header;

    while (1)
    {
        //监听其他进程向其发送命令的管道(阻塞)
        int count = epoll_wait(epfd,events,MAXEVENTS,NULL);
        for (int i = 0; i < count; i++)
        {
            //若该管道有命令写入
            if (events[i].events | EPOLLIN)
            {
                //读出相应的命令头
                recv(events[i].data.fd,&header,sizeof(UniformHeader),MSG_WAITALL);
                //将其插入命令队列的队尾
                cmd_queue.push(header);
                //读入命令
                recv(events[i].data.fd,buffer+)

            }
        }
        //若事件队列不为空
        if (!event_queue.empty())
        {
            //取出事件队列的第一个元素
            event_queue.pop(cmd);
            //执行对应的操作数据库命令
            result = do_cmd(cmd);
            //将命令执行结果传回相应的模块
            send(result, fifo);
        }
    }
}
