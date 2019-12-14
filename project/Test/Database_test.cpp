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
    DELETE_TEST,
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
    if(pid != 0 )
    {
        Database database(string("../config.conf"),string("../Log/database.log"));
        database.Run();
        kill(pid,SIGTERM);
    }
    else
    {

        fifo_ctod=open(path_fifo_ctod.c_str(),O_WRONLY);
        fifo_dtoc=open(path_fifo_dtoc.c_str(),O_RDONLY);
        
        fifo_rtod=open(path_fifo_rtod.c_str(),O_WRONLY);
        fifo_dtor=open(path_fifo_dtor.c_str(),O_RDONLY);

        fifo_stod=open(path_fifo_stod.c_str(),O_WRONLY);
        fifo_dtos=open(path_fifo_dtos.c_str(),O_RDONLY);

        //sleep(1);
        test = ALL;

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

            write(fifo_ctod,&header,sizeof(header));
            write(fifo_ctod,&body,sizeof(body));


            read(fifo_dtoc,&header,sizeof(header));
            read(fifo_dtoc,&res_body,sizeof(res_body));
            cout << "-------- SIGNUP TEST START------------"<<endl;


            cout<<"code:" <<res_body.code <<endl;
            cout<<"session:"<<res_body.Session <<endl;

            cout << "-------- SIGNUP TEST END------------"<<endl;
            cout <<endl;
        }

        if(test == SIGNIN_TEST || test == ALL)
        {
            UniformHeader header;
            header.len = sizeof(SigninBody);
            header.p = PackageType::SIGNIN;

            SigninBody body;
            SigninresBody res_body;
            strcpy(body.IP,"192.168.80.88");
            strcpy(body.Password,"202CB962AC59075B964B07152D234B70");
            strcpy(body.Username,"test");
            

            write(fifo_ctod,&header,sizeof(header));
            write(fifo_ctod,&body,sizeof(body));


            read(fifo_dtoc,&header,sizeof(header));
            read(fifo_dtoc,&res_body,sizeof(res_body));

            cout << "-------- SIGNIN TEST START------------"<<endl;


            cout<<"code:" <<res_body.code <<endl;
            cout<<"session:"<<res_body.Session <<endl;

            cout << "-------- SIGNIN TEST END------------"<<endl;
            cout<<endl;
        }

        if(test == UPLOAD_TEST || test == ALL)
        {
            UniformHeader header;
            header.len = sizeof(UploadReqBody);
            header.p = PackageType::UPLOAD_REQ;

            UploadReqBody body;
            UploadRespBody res_body;
            strcpy(body.fileName,"first_dir");
            body.fileSize = 0;
            body.isDir = 1;
            strcpy(body.MD5,"");
            strcpy(body.path,"/");
            strcpy(body.Session,"2");

            
            
            write(fifo_ctod,&header,sizeof(header));
            write(fifo_ctod,&body,sizeof(body));
            

            strcpy(body.fileName,"first_file");
            body.fileSize = 1024;
            body.isDir = 0;
            strcpy(body.MD5,"bf083d4ab960620b645557217dd59a49");
            strcpy(body.path,"/");
            strcpy(body.Session,"2");


            write(fifo_ctod,&header,sizeof(header));
            write(fifo_ctod,&body,sizeof(body));


            strcpy(body.fileName,"second_file");
            body.fileSize = 10000;
            body.isDir = 0;
            strcpy(body.MD5,"cb42e130d1471239a27fca6228094f0e");
            strcpy(body.path,"/");
            strcpy(body.Session,"2");


            write(fifo_ctod,&header,sizeof(header));
            write(fifo_ctod,&body,sizeof(body));


            cout << "-------- UPLOAD TEST START------------"<<endl;

            read(fifo_dtoc,&header,sizeof(header));
            read(fifo_dtoc,&res_body,sizeof(res_body));

            cout<<"code:" <<res_body.code <<endl;
            cout<<"session:"<<res_body.Session <<endl;

            read(fifo_dtoc,&header,sizeof(header));
            read(fifo_dtoc,&res_body,sizeof(res_body));

            cout<<"code:" <<res_body.code <<endl;
            cout<<"session:"<<res_body.Session <<endl;

            read(fifo_dtoc,&header,sizeof(header));
            read(fifo_dtoc,&res_body,sizeof(res_body));

            cout<<"code:" <<res_body.code <<endl;
            cout<<"session:"<<res_body.Session <<endl;

            cout << "-------- UPLOAD TEST END------------"<<endl;
            cout <<endl;

        }
        
        if(test == SYN_TEST || test == ALL)
        {
            UniformHeader header;
            header.len = sizeof(SYNReqBody);
            header.p = PackageType::SYN_REQ;

            SYNReqBody body;
            SYNRespBody res_body;
            strcpy(body.path,"/");
            strcpy(body.Session,"2");
            

            write(fifo_ctod,&header,sizeof(header));
            write(fifo_ctod,&body,sizeof(body));


            read(fifo_dtoc,&header,sizeof(header));
            read(fifo_dtoc,&res_body,sizeof(res_body));

            cout << "-------- SYN TEST START------------"<<endl;


            cout<<"code:" <<res_body.code <<endl;
            cout<<"session:"<<res_body.Session <<endl;
            cout <<"childnum:"<<res_body.childNum <<endl;
            cout <<"externalinfo:" <<res_body.ExternInformation <<endl;

            cout << "-------- SYN TEST START------------"<<endl;
            cout<<endl;
        }

        if(test == MOVE_TEST || test == ALL)
        {
            UniformHeader header;
            header.len = sizeof(MoveBody);
            header.p = PackageType::MOVE;

            MoveBody body;
            MoveRespBody res_body;
            strcpy(body.fileName,"first_file");
            strcpy(body.fileNameTo,"first_file");
            strcpy(body.path,"/");
            strcpy(body.pathTo,"/first_dir");
            strcpy(body.Session,"2");
            

            write(fifo_ctod,&header,sizeof(header));
            write(fifo_ctod,&body,sizeof(body));


            read(fifo_dtoc,&header,sizeof(header));
            read(fifo_dtoc,&res_body,sizeof(res_body));
            cout << "-------- MOVE TEST START------------"<<endl;


            cout<<"code:" <<res_body.code <<endl;
            cout<<"session:"<<res_body.Session <<endl;
            cout << "-------- MOVE TEST END------------"<<endl;
            cout<<endl;
        
        }

        if(test == COPY_TEST || test == ALL)
        {
            UniformHeader header;
            header.len = sizeof(CopyBody);
            header.p = PackageType::COPY;

            CopyBody body;
            CopyRespBody res_body;
            strcpy(body.fileName,"second_file");
            strcpy(body.fileNameTo,"second_file.bak");
            strcpy(body.path,"/");
            strcpy(body.pathTo,"/");
            strcpy(body.Session,"2");
            

            write(fifo_ctod,&header,sizeof(header));
            write(fifo_ctod,&body,sizeof(body));


            read(fifo_dtoc,&header,sizeof(header));
            read(fifo_dtoc,&res_body,sizeof(res_body));
            cout << "-------- COPY TEST START------------"<<endl;


            cout<<"code:" <<res_body.code <<endl;
            cout<<"session:"<<res_body.Session <<endl;

            cout << "-------- COPY TEST END------------"<<endl;
            cout <<endl;
        
        }

        if(test == DELETE_TEST || test == ALL)
        {
            UniformHeader header;
            header.len = sizeof(DeleteBody);
            header.p = PackageType::DELETE;

            DeleteBody body;
            DeleteRespBody res_body;
            strcpy(body.fileName,"second_file.bak");
            
            strcpy(body.path,"/");
           
            strcpy(body.Session,"2");
            

            write(fifo_ctod,&header,sizeof(header));
            write(fifo_ctod,&body,sizeof(body));


            read(fifo_dtoc,&header,sizeof(header));
            read(fifo_dtoc,&res_body,sizeof(res_body));

            cout << "-------- DELETE TEST START------------"<<endl;


            cout<<"code:" <<res_body.code <<endl;
            cout<<"session:"<<res_body.Session <<endl;

            cout << "-------- DELETE TEST START------------"<<endl;
            cout << endl;
        
        }
       
    }
    
    
    
    


    
}