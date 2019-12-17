#pragma once

#include "Package/Package.h"
#include "Config.h"
#include "Log.h"

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
