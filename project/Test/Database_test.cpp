#include "../database/Database.h"
#include <unistd.h>
#include <signal.h>
enum test_num{
    SIGNUP_TEST,
    SIGNIN_TEST,
    UPLOAD_TEST,
    SYN_TEST,
    MOVE_TEST,
    COPY_TEST,
    DELERE_TEST,
    ALL
};

int main()
{
    Config c("../config.conf");
    int test;
     
    string path_fifo_ctod = c.getValue("Global","path_fifo_ctod");
    string path_fifo_dtoc = c.getValue("Global","path_fifo_dtoc");
    string path_fifo_stod = c.getValue("Global","path_fifo_stod");
    string path_fifo_dtos = c.getValue("Global","path_fifo_dtos");
    string path_fifo_rtod = c.getValue("Global","path_fifo_rtod");
    string path_fifo_dtor = c.getValue("Global","path_fifo_dtor");

    unlink(path_fifo_ctod.c_str());
    unlink(path_fifo_dtoc.c_str());
    unlink(path_fifo_stod.c_str());
    unlink(path_fifo_dtos.c_str());
    unlink(path_fifo_rtod.c_str());
    unlink(path_fifo_dtor.c_str());

    int fifo_ctod,fifo_dtoc,fifo_stod,fifo_dtos,fifo_rtod,fifo_dtor;

    mkfifo(path_fifo_ctod.c_str(),S_IFIFO | 0666);
    mkfifo(path_fifo_dtoc.c_str(),S_IFIFO | 0666);
    mkfifo(path_fifo_stod.c_str(),S_IFIFO | 0666);
    mkfifo(path_fifo_dtos.c_str(),S_IFIFO | 0666);
    mkfifo(path_fifo_rtod.c_str(),S_IFIFO | 0666);
    mkfifo(path_fifo_dtor.c_str(),S_IFIFO | 0666);

    


    int pid = fork();
    if(pid ==0 )
    {
        cout << "child" <<endl;
        Database database(string("../config.conf"),string("../Log/database.log"));
        database.Run();
    }
    else
    {
        cout<<"parent"<<endl;
        fifo_ctod=open(path_fifo_ctod.c_str(),O_WRONLY);
        fifo_dtoc=open(path_fifo_dtoc.c_str(),O_RDONLY);
        
        fifo_rtod=open(path_fifo_rtod.c_str(),O_WRONLY);
        fifo_dtor=open(path_fifo_dtor.c_str(),O_RDONLY);

        fifo_stod=open(path_fifo_stod.c_str(),O_WRONLY);
        fifo_dtos=open(path_fifo_dtos.c_str(),O_RDONLY);

        sleep(1);
        if(test == SIGNUP_TEST || test == ALL)
        {
            UniformHeader header;
            header.len = sizeof(SignupBody);
            header.p = PackageType::SIGNUP;

            SignupBody body;
            SignupresBody res_body;
            strcpy(body.IP,"192.168.80.88");
            strcpy(body.Password,"202CB962AC59075B964B07152D234B70");
            strcpy(body.Username,"test");

            send(fifo_ctod,&header,sizeof(header),0);
            send(fifo_ctod,&body,sizeof(body),0);


            recv(fifo_dtoc,&header,sizeof(header),MSG_WAITALL);
            recv(fifo_dtoc,&res_body,sizeof(res_body),MSG_WAITALL);

            cout<<"code:" <<res_body.code <<endl;
            cout<<"session:"<<res_body.Session <<endl;
        }
        
        kill(pid,SIGTERM);
    }
    
    
    
    


    
}