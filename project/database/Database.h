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
    //����ʱ���
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
    /* ��ʼ�� mysql ������ʧ�ܷ���NULL */
    if ((mysql = mysql_init(NULL)) == NULL)
    {
        cout << "mysql_init failed" << endl;
        exit(-1);
    }

    /* �������ݿ⣬ʧ�ܷ���NULL
 *        1��mysqldû����
 *               2��û��ָ�����Ƶ����ݿ���� */
    if (mysql_real_connect(mysql, hostName.c_str(), userName.c_str(), password.c_str(), databaseName.c_str(), 0, NULL, 0) == NULL)
    {
        cout << "mysql_real_connect failed(" << mysql_error(mysql) << ")" << endl;
        exit(-1);
    }
    cout << "mysql connect sucessful" << endl;

    /* �����ַ���������������ַ����룬��ʹ/etc/my.cnf������Ҳ���� */
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
    /* ���в�ѯ���ɹ�����0�����ɹ���0
         1����ѯ�ַ��������﷨����
                2����ѯ�����ڵ����ݱ� */
    if (mysql_query(mysql, cmd.c_str()))
    {
        log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
    }
        /* ����ѯ����洢���������ִ����򷵻�NULL
        ע�⣺��ѯ���ΪNULL�����᷵��NULL */
    if ((result = mysql_store_result(mysql)) == NULL)
    {
        log.writeLog(Log::ERROR,string("mysql_store_result failed"));
    }
    
    // ���Ҳ������û�
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
    //����˳�� Uname,Password,IP,Lastlogintime,Createtime
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
        //���������������䷢������Ĺܵ�(����)
        int count = epoll_wait(epfd, events, MAXEVENTS, NULL);
        if (count < 0)
        {
            perror("epoll wait error");
        }
        for (int i = 0; i < count; i++)
        {
            //���ùܵ�������д��
            if (events[i].events | EPOLLIN)
            {
                //������Ӧ������ͷ
                int len = recv(events[i].data.fd, &header, sizeof(UniformHeader), MSG_WAITALL);
                if (len <= 0)
                {
                    log.writeLog(Log::ERROR, "recv() error when read header");
                }

                //������Ӧ��С�Ŀռ�
                void *temp = new char[header.len];

                //��������
                len = recv(events[i].data.fd, temp, header.len, MSG_WAITALL);
                if (len <= 0)
                {
                    log.writeLog(Log::ERROR, "recv() error when rcve body");
                }

                //�����������ͷ���еĶ�β
                header_queue.push(header);

                //������body���ֵ�ָ�����cmd_queue��β��
                cmd_queue.push(temp);
            }
        }
        //���¼����в�Ϊ��
        if (!header_queue.empty())
        {
            //ȡ���¼����еĵ�һ��Ԫ��
            header = header_queue.front();

            //�������Ԫ��
            cmd_queue.pop();

            //ִ�ж�Ӧ�Ĳ������ݿ�����
            do_mysql_cmd(header);

            //������ִ�н��������Ӧ��ģ��
            //send(result, fifo);
        }
    }
}

int Database::do_mysql_cmd(UniformHeader h)
{
    string cmd;
    UniformHeader res_header;
    
    //��Ϊ��½�¼�
    if (h.p == PackageType::SIGNIN)
    {
        //get cmd body
        SigninBody *body = (SigninBody *)cmd_queue.front();
        string user_pwd;

        //���ɷ��ذ���ͷ
        res_header.len = sizeof(SigninresBody);
        res_header.p = PackageType::SIGNIN_RES;
        res_header_queue.push(res_header);

        //���ɷ��ذ�����
        SigninresBody *res = new SigninresBody[1];
        strcpy(res->Session,body->Session);

        //�����ݿ��Ҹ��û�
        user_pwd = get_pwd_by_uname(body->Username);

        //�û�������
        if(user_pwd == "NULL")
        {
            res->code = SIGNIN_UNEXIST_USERNAME;
        }
        //�û�����
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
        
        //��������д���� 
        res_queue.push(res);

        //�ͷ��ڴ�
        delete body;
    }

    //��Ϊע���¼�
    else if (h.p == PackageType::SIGNUP)
    {
        SignupBody *body = (SignupBody *)cmd_queue.front();
        
        string user_pwd;

        //���ɷ��ذ���ͷ
        res_header.len = sizeof(SignupresBody);
        res_header.p = PackageType::SIGNUP_RES;
        res_header_queue.push(res_header);

         //���ɷ��ذ�����
        SignupresBody *res = new SignupresBody[1];
        
      
        user_pwd = get_pwd_by_uname(body->Username);

        //δ�ҵ����û�
        if(user_pwd == "NULL")
        {
            //Ϊ���û������µı���
            insert_Users(body->Username,MD5(body->Password),body->IP);
            res->code = SIGNUP_SUCCESS;
        }
        else
        {
            //��֪�û����û����Ѵ��� 
            res->code = SIGNUP_EXISTED_USERNAME;
        }
        
        //��������д���� 
        res_queue.push(res);

    }

    //��Ϊ�ļ��ϴ����� ͬ���ļ��ĸ��ǻ�û��
    else if(h.p == PackageType::UPLOAD_REQ)
    {
        UploadReqBody *body = (UploadReqBody *)cmd_queue.front();
        //�����Ƿ������ͬ�ļ�
        cmd = "select * from Files where Hash="+string(body->MD5);
        string fileName = body->fileName;
        bool already_exist = 0;
        char bitmap[65535];
        //init bitmap
        memset(bitmap,sizeof(bitmap),"0");

        //���ɷ��ذ���ͷ
        res_header.len = sizeof(UploadRespBody);
        res_header.p = PackageType::UPLOAD_RESP;
        res_header_queue.push(res_header);

         //���ɷ��ذ�����
        UploadRespBody *res = new UploadRespBody[1];

       
        if (mysql_query(mysql, cmd.c_str()))
        {
            log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        }
       
        if ((result = mysql_store_result(mysql)) != NULL)
        {
            log.writeLog(Log::ERROR,string("mysql_store_result failed"));
        }
        
        // ��û���ҵ���ͬ�ļ�
        if((int)mysql_num_rows(result) != 0)
        {
           already_exist = 1;
        }  

        
        //����file��
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

        //��д����FileIndex������
        if(!already_exist){
            cmd = "inster into FileIndex values("+ \
            "\""+string(MD5(body->MD5))+"\""+","+   \
            "0"+","+"\""+string(bitmap)+"\""+");";
        }
        else{
            //����������
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

        //����FileIndex��
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
        //��������д���� 
        res_queue.push(res);
    }

    //��Ϊ�ļ�Ŀ¼ͬ������
    else if(h.p == PackageType::SYN_REQ)
    {
        SYNReqBody *body = (SYNReqBody *)cmd_queue.front(); 
        string cmd = "select Filename,Isdir from Files where Uid="+string(body->Session)+" and Dir="+"\""+string(body->path)+"\""+";";

        //���ɷ��ذ���ͷ
        res_header.len = sizeof(SYNRespBody);
        res_header.p = PackageType::UPLOAD_RESP;
        res_header_queue.push(res_header);

         //���ɷ��ذ�����
        SYNRespBody *res = new SYNRespBody[1];
        
        //ִ�в�ѯ���
        if (mysql_query(mysql, cmd.c_str()))
        {
            log.writeLog(Log::ERROR,string("mysql_query failed(")+string(mysql_error(mysql))+string(")"));
        }

        //��ȡ��ѯ���
        if ((result = mysql_store_result(mysql)) != NULL)
        {
            log.writeLog(Log::ERROR,string("mysql_store_result failed"));
        }

        res->childNum = (int)mysql_num_rows(result);

        memset(res->ExternInformation,sizeof(res->ExternInformation),0);
        //�������ҽ��
        while ((row = mysql_fetch_row(result)) != NULL)
        {
            //�洢�ṹ [�ļ�/�ļ�����,�Ƿ�Ϊ�ļ���] �ո� [�ļ�/�ļ�����,�Ƿ�Ϊ�ļ���]...
           strcat(res->ExternInformation,row[0]);
           strcat(res->ExternInformation,",");
           strcat(res->ExternInformation,row[1]);
           strcat(res->ExternInformation," ");
        }
        res->code = SYN_SUCCESS;

        //��������д����
        res_queue.push(res);
    }

    //��Ϊ�ļ�/�ļ��п�������
    else if(h.p == PackageType::COPY)
    {
        
    }

    cmd_queue.pop();
}
