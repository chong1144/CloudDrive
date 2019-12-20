// #include "Database.h"
#include "root_dir.h"
#include "signal.h"
#include "FileOperations.h"

int main(int argc, char const *argv[])
{
    string configFile =  string(root) + "/Config/config.conf";
    Config c(configFile);

    string logFile =   string(root) + "/Log/test.log";
    Log l(logFile);

    FileOperations F(configFile);

    //l.writeLog(l.ERROR, "Nothing");
    // Database db;
    //test_database(c,l);
    return 0;
}



// void test_database(Config c,Log l)
// {
//     enum test_num{
//         SIGNUP_TEST,
//         SIGNIN_TEST,
//         UPLOAD_TEST,
//         SYN_TEST,
//         MOVE_TEST,
//         COPY_TEST,
//         DELERE_TEST,
//         MKDIR_TEST,
//         FILEINFO_TEST,
//         ALL
//     };

//     int test;
     
//     string path_fifo_ctod = c.getValue("Global","path_fifo_ctod");
//     string path_fifo_dtoc = c.getValue("Global","path_fifo_dtoc");
//     string path_fifo_stod = c.getValue("Global","path_fifo_stod");
//     string path_fifo_dtos = c.getValue("Global","path_fifo_dtos");
//     string path_fifo_rtod = c.getValue("Global","path_fifo_rtod");
//     string path_fifo_dtor = c.getValue("Global","path_fifo_dtor");

//     unlink(path_fifo_ctod.c_str());
//     unlink(path_fifo_dtoc.c_str());
//     unlink(path_fifo_stod.c_str());
//     unlink(path_fifo_dtos.c_str());
//     unlink(path_fifo_rtod.c_str());
//     unlink(path_fifo_dtor.c_str());

//     int fifo_ctod,fifo_dtoc,fifo_stod,fifo_dtos,fifo_rtod,fifo_dtor;

//     mkfifo(path_fifo_ctod.c_str(),S_IFIFO | 0666);
//     mkfifo(path_fifo_dtoc.c_str(),S_IFIFO | 0666);
//     mkfifo(path_fifo_stod.c_str(),S_IFIFO | 0666);
//     mkfifo(path_fifo_dtos.c_str(),S_IFIFO | 0666);
//     mkfifo(path_fifo_rtod.c_str(),S_IFIFO | 0666);
//     mkfifo(path_fifo_dtor.c_str(),S_IFIFO | 0666);

    


//     int pid = fork();
//     if(pid != 0 )
//     {
//         //Database db;
//         //database.Run();
//         kill(pid,SIGTERM);
//     }
//     else
//     {
//         fifo_ctod=open(path_fifo_ctod.c_str(),O_WRONLY);
//         fifo_dtoc=open(path_fifo_dtoc.c_str(),O_RDONLY);
        
//         fifo_rtod=open(path_fifo_rtod.c_str(),O_WRONLY);
//         fifo_dtor=open(path_fifo_dtor.c_str(),O_RDONLY);

//         fifo_stod=open(path_fifo_stod.c_str(),O_WRONLY);
//         fifo_dtos=open(path_fifo_dtos.c_str(),O_RDONLY);

//         sleep(1);
//         test = UPLOAD_TEST;

//         if(test == SIGNUP_TEST || test == ALL)
//         {
//             UniformHeader header;
//             header.len = sizeof(SignupBody);
//             header.p = PackageType::SIGNUP;

//             SignupBody body;
//             SignupresBody res_body;
//             strcpy(body.IP,"192.168.80.88");
//             strcpy(body.Password,"202CB962AC59075B964B07152D234B70");
//             strcpy(body.Username,"test");

//             write(fifo_ctod,&header,sizeof(header));
//             write(fifo_ctod,&body,sizeof(body));


//             read(fifo_dtoc,&header,sizeof(header));
//             read(fifo_dtoc,&res_body,sizeof(res_body));

//             cout<<"code:" <<res_body.code <<endl;
//             cout<<"session:"<<res_body.Session <<endl;
//         }

//         if(test == SIGNIN_TEST || test == ALL)
//         {
//             UniformHeader header;
//             header.len = sizeof(SigninBody);
//             header.p = PackageType::SIGNIN;

//             SigninBody body;
//             SigninresBody res_body;
//             strcpy(body.IP,"192.168.80.88");
//             strcpy(body.Password,"d9840773233fa6b19fde8caf765402f5");
//             strcpy(body.Username,"test");
            

//             write(fifo_ctod,&header,sizeof(header));
//             write(fifo_ctod,&body,sizeof(body));


//             read(fifo_dtoc,&header,sizeof(header));
//             read(fifo_dtoc,&res_body,sizeof(res_body));

//             cout<<"code:" <<res_body.code <<endl;
//             cout<<"session:"<<res_body.Session <<endl;
//         }

//         if(test == UPLOAD_TEST || test == ALL)
//         {
//             UniformHeader header;
//             header.len = sizeof(UploadReqBody);
//             header.p = PackageType::UPLOAD_REQ;

//             UploadReqBody body;
//             UploadRespBody res_body;
//             strcpy(body.fileName,"my_first_file");
//             body.fileSize = 100;
//             body.isDir = 0;
//             strcpy(body.MD5,"bf083d4ab960620b645557217dd59a49");
//             strcpy(body.path,"/");
//             strcpy(body.Session,"4");
            

//             write(fifo_ctod,&header,sizeof(header));
//             write(fifo_ctod,&body,sizeof(body));


//             read(fifo_dtoc,&header,sizeof(header));
//             read(fifo_dtoc,&res_body,sizeof(res_body));

//             cout<<"code:" <<res_body.code <<endl;
//             cout<<"session:"<<res_body.Session <<endl;
//         }

//         if(test == UPLOAD_TEST || test == ALL)
//         {
//             UniformHeader header;
//             header.len = sizeof(UploadReqBody);
//             header.p = PackageType::UPLOAD_REQ;

//             UploadReqBody body;
//             UploadRespBody res_body;
//             strcpy(body.fileName,"my_first_file");
//             body.fileSize = 100;
//             body.isDir = 0;
//             strcpy(body.MD5,"bf083d4ab960620b645557217dd59a49");
//             strcpy(body.path,"/");
//             strcpy(body.Session,"4");
            

//             write(fifo_ctod,&header,sizeof(header));
//             write(fifo_ctod,&body,sizeof(body));


//             read(fifo_dtoc,&header,sizeof(header));
//             read(fifo_dtoc,&res_body,sizeof(res_body));

//             cout<<"code:" <<res_body.code <<endl;
//             cout<<"session:"<<res_body.Session <<endl;
//         }

//         if(test == MKDIR_TEST || test == ALL)
//         {
//             UniformHeader header;
//             header.len = sizeof(MkdirBody);
//             header.p = PackageType::MKDIR;

//             MkdirBody body;
//             MkdirRespBody res_body;
//             strcpy(body.dirName,"my_first_dir");
            
//             strcpy(body.path,"/");
//             strcpy(body.Session,"4");
            

//             write(fifo_ctod,&header,sizeof(header));
//             write(fifo_ctod,&body,sizeof(body));


//             read(fifo_dtoc,&header,sizeof(header));
//             read(fifo_dtoc,&res_body,sizeof(res_body));
//             cout<<"code:" <<res_body.code <<endl;
//             cout<<"session:"<<res_body.Session <<endl;
//         }
        
       
//     }
    

// }
