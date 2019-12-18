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
    char md5[64];
    uint16_t chunkNo;
    int size;
    char content[1024 * 1024];
};

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

    // �����Ƿ��и��ļ�
    FileStatusCodes IsExists(const MD5Code &md5);
    // �����ļ���
    FileStatusCodes Mkdir(const MD5Code &md5);
    // д����Ӧ���ļ�
    int WriteFile(const MD5Code &md5, const UploadPushBody &packet);

    int WriteFile (const FileChunk& filechunk);
    // ��ȡ��Ӧ�ļ�
    int ReadFile(const MD5Code &md5, const uint16_t &id, DownloadPushBody &packet);
};