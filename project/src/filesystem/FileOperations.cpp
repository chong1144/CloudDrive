#include "FileOperations.h"

FileOperations::FileOperations(const string &ConfigFile)
{
    Config c(ConfigFile);

    prefixPos = c.getValue("WriteRead", "prefix");

    l.init(c.getValue("WriteRead", "fileSystemLogPosition"));
}

FileOperations::~FileOperations()
{
}

FileStatusCodes FileOperations::IsExists(const MD5Code &md5)
{
    DIR *fp = opendir(this->prefixPos.c_str());
    if (fp == NULL)
    {
        string ErrorContent = "Fail to look through " + this->prefixPos;
        l.writeLog(Log::ERROR, ErrorContent);
        exit(-1);
    }

    dirent *entry;
    while ((entry = readdir(fp)))
    {
		if (!strncmp (entry->d_name, md5, 32))
            return FILE_EXISTS;
    }

    return FILE_UNEXISTS;
}

FileStatusCodes FileOperations::Mkdir(const MD5Code &md5)
{
    const string DirPath = this->prefixPos + md5;
    if (mkdir(DirPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
    {
        string ErrorContent = "Fail to mkdir " + DirPath;
        l.writeLog(Log::ERROR, ErrorContent);
        return MKDIR_FAIL;
    }

    return MKDIR_SUCCESS;
}


/*
* Note：如果该文件夹不存在则新建文件夹
* 路径：md5/packet.id
*/
int FileOperations::WriteFile(const MD5Code &md5, const UploadPushBody &packet)
{
    // 如果该文件夹不存在则新建文件夹
    if (this->IsExists(md5) == FILE_UNEXISTS)
    {
        if(this->Mkdir(md5) == MKDIR_FAIL)
        {
            // safety gaurdence
            exit(-1);
        }
    }

    // 写入文件的位置
    const string filePath = this->prefixPos + string(md5) + "/" + std::to_string(packet.id);
    ofstream fout(filePath.c_str(), ios::out | ios::binary);
    if (!fout.is_open())
    {
        string ErrorContent = "Fail to open " + filePath;
        l.writeLog(Log::ERROR, ErrorContent);
        return WRITE_FAIL;
    }
    
    fout.write(packet.content, packet.len);
    fout.close();

    string InfoContent = "Write " + filePath + " successfully!";
    l.writeLog(Log::INFO, InfoContent);

    return packet.len;
}

int FileOperations::WriteFile (const FileChunk& filechunk)
{ 
    // 如果该文件夹不存在则新建文件夹
    if (this->IsExists (filechunk.md5) == FILE_UNEXISTS) {
        l.writeLog (Log::INFO, string ("MKDIR_FAIL"));
		if (this->Mkdir (filechunk.md5) == MKDIR_FAIL) {
            // safety gaurdence
            l.writeLog (Log::ERROR, string ("MKDIR_FAIL EXIT!!!"));
            exit (-1);
        }
    }

    // 写入文件的位置
    const string filePath = this->prefixPos + string (filechunk.md5) + "/" + std::to_string (filechunk.chunkNo);
    ofstream fout (filePath.c_str (), ios::out | ios::binary);
    if (!fout.is_open ()) {
        string ErrorContent = "Fail to open " + filePath;
        l.writeLog (Log::ERROR, ErrorContent);
        return WRITE_FAIL;
    }

    fout.write (filechunk.content, filechunk.size);
    fout.close ();

    string InfoContent = "Write " + filePath + " successfully!";
    l.writeLog (Log::INFO, InfoContent);
    return filechunk.size;
}


/*
* 
* 
*/
int FileOperations::ReadFile(const MD5Code &md5, const uint16_t &id, DownloadPushBody &packet)
{
    // 未能找到相应文件
    if(this->IsExists(md5) == FILE_UNEXISTS)
    {
        string ErrorContent = "Fail to find " + this->prefixPos + string(md5);
        l.writeLog(Log::ERROR, ErrorContent);
        return FILE_UNEXISTS;
    }

    // 读入文件的位置
    const string filePath = this->prefixPos + string(md5) + "/" + std::to_string(id);
    ifstream fin(filePath.c_str(), ios::in | ios::binary);
    if(!fin.is_open())
    {
        string ErrorContent = "Fail to open " + filePath;
        l.writeLog(Log::ERROR, ErrorContent);
        return FILE_UNEXISTS;
    }
    uint16_t readSize = 0;
    fin.read(packet.content, ChunkSize);
    readSize = fin.gcount();
    fin.close();

    string InfoContent = "Read " + filePath + " successfully!";
    l.writeLog(Log::INFO, InfoContent);

    return readSize;
}