#pragma once

#include "../Package/Package.h"
#include "../Utility/Config.h"
#include "../Utility/Log.h"

#include <string>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>

using MD5Code = char[MD5Length];
using Content = char[ChunkSize];
using std::ifstream;
using std::ios;
using std::ofstream;
using std::string;

enum FileStatusCodes
{
    FILE_EXISTS = 1,
    FILE_UNEXISTS = -1,
    WRITE_FAIL = -2,
    MKDIR_FAIL = -3,
    MKDIR_SUCCESS = 2
};

const string fileSystemLogPosition = "../Log/filesystem.log";

class FileOperations
{
private:
    string prefixPos;
    Log l;

public:
    FileOperations(const string &ConfigFile);
    ~FileOperations();

    // 查找是否有该文件
    FileStatusCodes IsExists(const MD5Code &md5);
    // 建立文件夹
    FileStatusCodes Mkdir(const MD5Code &md5);
    // 写入相应的文件
    uint16_t WriteFile(const MD5Code &md5, const UploadPushBody &packet);
    // 读取相应文件
    uint16_t ReadFile(const MD5Code &md5, const uint16_t &id, DownloadPushBody &packet);
};

FileOperations::FileOperations(const string &ConfigFile)
{
    Config c(ConfigFile);

    prefixPos = c.getValue("WriteRead", "prefix");

    l.init(fileSystemLogPosition);
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
        if (!strcmp(entry->d_name, md5))
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

uint16_t FileOperations::WriteFile(const MD5Code &md5, const UploadPushBody &packet)
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

uint16_t FileOperations::ReadFile(const MD5Code &md5, const uint16_t &id, DownloadPushBody &packet)
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