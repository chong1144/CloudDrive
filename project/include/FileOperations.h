#pragma once

#include "Package/Package.h"
#include "Config.h"
#include "Log.h"

#include <string>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <memory>
using MD5Code = char[MD5Length];
using Content = char[ChunkSize];
using std::ifstream;
using std::ios;
using std::ofstream;
using std::string;
using std::unique_ptr;
struct FileChunk
{
    char md5[MD5Length];
    uint16_t chunkNo;
    int size;
    char content[1024 * 1024];
};

enum FileStatusCodes
{
    FFILE_EXISTS = 1,
    FFILE_UNEXISTS = -1,
    FWRITE_FAIL = -2,
    FMKDIR_FAIL = -3,
    FMKDIR_SUCCESS = 2
};


class FileOperations
{
private:
    string prefixPos;
    Log l;

public:
    // 默认构造函数，但是需要使用init完成初始化
    FileOperations();
    FileOperations(const string &ConfigFile);
    ~FileOperations();

    void init(const string& ConfigFile);
    // 查找是否有该文件
    FileStatusCodes IsExists(const MD5Code &md5);
    // 建立文件夹
    FileStatusCodes Mkdir(const MD5Code &md5);
    // 写入相应的文件
    int WriteFile(const MD5Code &md5, const UploadPushBody &packet);

    int WriteFile (const FileChunk& filechunk);
    // 读取相应文件
    int ReadFile(const MD5Code &md5, const uint16_t &id, DownloadPushBody &packet);

    int ReadFile(FileChunk& filechunk);
};
