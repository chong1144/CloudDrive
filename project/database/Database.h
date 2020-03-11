#include "../Package/Package.h"

#include <iostream>
#include <queue>
#include <stdlib.h>
#include <string>
#include <mysql/mysql.h>
#include <sys/epoll.h>

using std::queue;
using std::string;

//table names
#define FILES "Files"
#define FILEINDEX "FileIndex"
#define USERS "Users"

#define BUF_SIZE 1024*1024


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
    //mysql configs
    string DatabaseName;
    string passWord;
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
    queue<DatabaseEvent> events;

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
    Database(/* args */);
    int Run();
    ~Database();
};

//connect database
Database::Database(/* args */)
{
}

//disconnect from database
Database::~Database()
{
}

int Database::Run()
{

    int epoll_fd = epoll_create1(0);
    while (1)
    {
        //���������������䷢������Ĺܵ�(����)
        int count = epoll_wait(fifos);
        for (i = 0; i < count; i++)
        {
            //���ùܵ�������д��
            if (fifos[i] == input)
            {
                //������Ӧ������
                read(cmd, sizeof(cmd));
                //��������¼����еĶ�β
                event_queue.push(cmd);
            }
        }
        //���¼����в�Ϊ��
        if (!event_queue.empty())
        {
            //ȡ���¼����еĵ�һ��Ԫ��
            event_queue.pop(cmd);
            //ִ�ж�Ӧ�Ĳ������ݿ�����
            result = do_cmd(cmd);
            //������ִ�н��������Ӧ��ģ��
            send(result, fifo);
        }
    }
}
