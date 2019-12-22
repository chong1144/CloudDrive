#include "Database.h"

string generate_string(string src)
{
    
    string t = "\"";
    t += src;
    t += "\"";
    return t;
    
}

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
    // fifo_stod = open(config.getValue("Global", "path_fifo_stod").c_str(), O_RDONLY);
    // fifo_dtos = open(config.getValue("Global", "path_fifo_dtos").c_str(), O_WRONLY);
     
    if (fifo_ctod < 0 || fifo_dtoc < 0 || fifo_rtod < 0 || fifo_dtor < 0 || fifo_stod < 0 || fifo_dtor < 0)
    {
        cout << "open fifo error" << endl;
        exit(-1);
    }
    //cout << "open fifo success"<<endl;
    log.writeLog(Log::INFO,"open fifo success");

    memset(bitmapRecvBuf,0,BITMAP_SIZE*3);
    memset(bitmapSendBuf,0,BITMAP_SIZE*3);
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

bool Database::dir_exist(string Uid,string dirName,string path)
{
     string cmd = "select * from Files where Uid="+ \
    generate_string(Uid)+" and "+\
    "Filename="+generate_string(dirName)+" and "+\
    "Path="+generate_string(path)+";";


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

string Database::Files_Get_Time(string Uid,string Filename,string path)
{
    string cmd = "select Modtime from Files where Uid="+ \
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
    string hash = Files_Get_Hash(Uid,Filename,path);
    if(hash == "NULL")
    {
        return 0;
    }

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
    if(hash == "NULL")
    {
        return 0;
    }
    bool isDir = Files_Isdir(Uid,Filename,path);

    FileIndex_Refinc(hash);

    return Files_Insert(Uid,FilenameTo,pathTo,hash,isDir);

}

int Database::Files_Move(string Uid,string Filename,string path,string FilenameTo ,string pathTo)
{

    string hash = Files_Get_Hash(Uid,Filename,path);
    if(hash == "NULL")
    {
        return 0;
    }

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

int Database::FileIndex_GetSize(string hash)
{
    //char Bitmap[BITMAP_SIZE];
    int size;

    string cmd = string("select Size from FileIndex where ")+ \
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
        return -1;
    }
    else
    {
        return atoi(row[0]);
    }
    
    

}

int Database::FileIndex_UpdateBitmap(string hash,string Bitmap)
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
    epoll_event events[DATABASE_MAXEVENTS], ep_ev;
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
        int count = epoll_wait(epfd, events, DATABASE_MAXEVENTS, -1);
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
                if(header.p==PackageType::FILEINFO)
                {
                    FileInfoBody *p = (FileInfoBody *)(temp);
                    len = read(events[i].data.fd,(char *)bitmapRecvBuf+strlen((char *)bitmapRecvBuf),p->size);
                    if(len<=0)
                    {
                        log.writeLog(Log::ERROR, string("fifo closed ")+to_string(events[i].data.fd));
                        exit(-1);
                    }
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
        //strcpy(res->Session,"");
        memset(res->Session,0,sizeof(res->Session));

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
                memset(res->Session, 0, sizeof(res->Session));
                
                strcpy(res->Session,uid_pwd.first.c_str());
                log.writeLog(Log::INFO, uid_pwd.first + " "+res->Session);
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
            memset(res->Session, 0, sizeof(res->Session));
            strcpy(res->Session,uid_pwd.first.c_str());

            log.writeLog(Log::INFO,"[Register Success]");
        }
        else
        {
            //告知用户该用户名已存在 

            res->code = SIGNUP_EXISTED_USERNAME;
            memset(res->Session, 0, sizeof(res->Session));
            strcpy(res->Session,"");
            log.writeLog(Log::INFO,"[Register Failed]");
        }
        
        //将包加入写队列 
        res_queue.push(res);

       
        //释放内存
        
        delete body;
        
    }

    //若为文件上传请求 
    else if(h.p == PackageType::UPLOAD_REQ)
    {
        UploadReqBody *body = (UploadReqBody *)cmd_queue.front();

        //生成返回包的头
        //包的长度此时还无法计算

        log.writeLog(Log::INFO, "[Upload Req] Uid:"+string(body->Session)+", FileName:"+string(body->fileName)+", Filesize:"+to_string(body->fileSize)+", Path:"+string(body->path));


        //生成包头
        res_header.len = sizeof(FileInfoBody);
        res_header.p = PackageType::FILEINFO;
        res_header_queue.push(res_header);

         //生成返回包的身
        FileInfoBody *res = new FileInfoBody[1];
        memset(res->md5,0,sizeof(res->md5));
        strcpy(res->md5,body->MD5);
    
        

        //若文件已存在
        if(file_exist(body->MD5))
        {
            FileIndex_Refinc(body->MD5);
            
            
            pair<int,int> ref_complete =  FileIndex_Get_Ref_Complete(body->MD5);
           
            res->completed = ref_complete.second;
           
            
            log.writeLog(Log::INFO,"[Upload Req Success] ALREADY_HAS or uploading");
        } 
        else
        {
            
            FileIndex_Insert(body->MD5,body->fileSize);
            res->completed = 0;
            log.writeLog(Log::INFO,"[Upload Req Success]");
        }
        Files_Insert(body->Session,body->fileName,body->path,body->MD5,0);


        string bitmap = FileIndex_GetBitmap(body->MD5);
        res->exist = 1;
        res->size = bitmap.size();
       
        //将包加入写队列
        res_queue.push(res);

        //将bitmap 写入缓冲区
        strcat(bitmapSendBuf,bitmap.c_str());
         
        delete body;
         
    }

    //若为新建文件夹请求 
    else if(h.p == PackageType::MKDIR)
    {
        MkdirBody *body = (MkdirBody *)cmd_queue.front();


        log.writeLog(Log::INFO, "[Mkdir Req] Uid:"+string(body->Session)+", DirName:"+string(body->dirName)+", Path:"+string(body->path));


         //生成返回包的头
        res_header.len = sizeof(MkdirRespBody);
        res_header.p = PackageType::MKDIR_RES;
        res_header_queue.push(res_header);
        
        MkdirRespBody *res = new MkdirRespBody[1];
        memset(res->Session,0,sizeof(res->Session));
        strcpy(res->Session,body->Session);
        

        //若文件已存在
        if(dir_exist(body->Session,body->dirName,body->path))
        {
            
            res->code = MKDIR_ALREADY_HAS;
            log.writeLog(Log::INFO,"[Mkdir Failed] ALREADY_HAS ");
        } 
        else
        {
            
            Files_Insert(body->Session,body->dirName,body->path,"",1);
            res->code = MKDIR_SUCCESS;
            log.writeLog(Log::INFO,"[Mkdir Req Success]");
        }
        
        //将包加入写队列
        res_queue.push(res);
       
        delete body;
    }

    //若为文件目录同步请求
    //返回格式，先返回一个resp包，包里面是孩子节点个数n,然后发送n个SYNpush包
    else if(h.p == PackageType::SYN_REQ)
    {
        SYNReqBody *body = (SYNReqBody *)cmd_queue.front(); 
        //生成返回包的头
        res_header.len = sizeof(SYNRespBody);
        res_header.p = PackageType::SYN_RESP;
        res_header_queue.push(res_header);

         //生成返回包的身
        SYNRespBody *res = new SYNRespBody[1];
        memset(res->Session,0,sizeof(res->Session));
        strcpy(res->Session,body->Session);

        log.writeLog(Log::INFO, "[SYN Req] Uid:"+string(body->Session)+", path:"+string(body->path));

        vector<pair<string,string>> children;
        
        children = get_child_files(body->Session,body->path);

        res->childNum = children.size();
        
        
       
        res->code = SYN_SUCCESS;
        log.writeLog(Log::INFO,"[SYN Req Success]");
        //将包加入写队列
        res_queue.push(res);

        res_header.len = sizeof(SYNPushBody);
        res_header.p = PackageType::SYN_PUSH;

        int i = 0;
        while(!children.empty())
        {
            UniformHeader header;
            header.len = sizeof(SYNPushBody);
            header.p = PackageType::SYN_PUSH;
            res_header_queue.push(header);

            pair<string,string> filename_isdir = children.back();
            children.pop_back();
            SYNPushBody *p = new SYNPushBody[1];
            p->id = i++;
            memset(p->Session,0,sizeof(p->Session));
            strcpy(p->Session,body->Session);
            memset(p->name,0,sizeof(p->name));
            strcpy(p->name,filename_isdir.first.c_str());
            p->isFile = !atoi(filename_isdir.second.c_str());

            if(p->isFile)
            {
                string t = Files_Get_Hash(body->Session,p->name,body->path);
                p->size = FileIndex_GetSize(t);
                
            }
            else
            {
                p->size = 0;
            }
            memset(p->time,0,sizeof(p->time));
            strcpy(p->time,Files_Get_Time(body->Session,p->name,body->path).c_str());


            res_queue.push(p);
            log.writeLog(Log::INFO, string("p->Session=")+p->Session);
            log.writeLog(Log::INFO, string("p->name=")+p->name);
            log.writeLog(Log::INFO, string("p->id=")+to_string(p->id));
            log.writeLog(Log::INFO, string("p->size=")+to_string(p->size));
            log.writeLog(Log::INFO, string("p->time=")+p->time);
            log.writeLog(Log::INFO, string("p->isFile=")+to_string(p->isFile));

            log.writeLog(Log::INFO,"write SYNPushbody");
        }

        delete body;
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
        memset(res->Session,0,sizeof(res->Session));
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
        memset(res->Session,0,sizeof(res->Session));
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
        memset(res->Session,0,sizeof(res->Session));
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

    
     //若为更新文件信息
    else if(h.p == PackageType::FILEINFO)
    {
        FileInfoBody *body = (FileInfoBody *)cmd_queue.front();

        //不需要返回

        log.writeLog(Log::INFO, "[FileInfo Req] md5:"+string(body->md5));

        char bitmap[BITMAP_SIZE];
        memset(bitmap,0,BITMAP_SIZE);

        //从缓冲区获取bitmap
        memcpy(bitmap,bitmapRecvBuf,body->size);

        //移动缓冲区
        memmove(bitmapRecvBuf,bitmapRecvBuf+body->size,sizeof(bitmapRecvBuf)-body->size);

        if(bitmap=="NULL")
        {
            log.writeLog(Log::ERROR,"[FileInfo update error]");
        }
        else
        {
            log.writeLog(Log::WARNING,"[FileInfo update success]");
        }
                
        FileIndex_UpdateBitmap(body->md5,bitmap);

      

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
        SYNRespBody *body = (SYNRespBody*)res_queue.front();
        write(fifo_dtoc,&header,sizeof(header));
        write(fifo_dtoc,body,sizeof(SYNRespBody));
        
        delete body;
    }
    else if(header.p == PackageType::SYN_PUSH)
    {
        SYNPushBody *body = (SYNPushBody *)res_queue.front();
        write(fifo_dtoc,&header,sizeof(header));
        write(fifo_dtoc,body,sizeof(SYNPushBody));
        log.writeLog(Log::INFO,"send SYNPUSH");
        delete body;
    }

    else if (header.p == PackageType::MKDIR_RES)
    {
        MkdirRespBody *body = (MkdirRespBody*)res_queue.front();
        write(fifo_dtoc,&header,sizeof(header));
        write(fifo_dtoc,body,sizeof(MkdirRespBody));
        delete body;
    }
    else if (header.p == PackageType::FILEINFO)
    {
        FileInfoBody *body = (FileInfoBody*)res_queue.front();
        write(fifo_dtor,&header,sizeof(header));
        write(fifo_dtor,body,sizeof(FileInfoBody));
        write(fifo_dtor,bitmapSendBuf,body->size);
        memmove(bitmapSendBuf,bitmapSendBuf+body->size,sizeof(bitmapSendBuf) - (body->size));
        
        delete body;
        
    }

    res_queue.pop();
}
