#pragma once

#include "../Package/Package.h"
#include "../Utility/Config.h"

#include <string>
#include 

using MD5Code = char[MD5Length];
using Content = char[ChunkSize];
using std::string;

enum FileStatusCodes
{
    FILE_EXISTS = 1,
    FILE_UNEXISTS = -1
};

class FileOperations
{
private:
    string prefixPos;

public:
    FileOperations(const string &ConfigFile);

    // �����Ƿ��и��ļ�
    FileStatusCodes IsExists(const MD5Code &md5);

    // д����Ӧ���ļ�
    uint16_t WriteFile(const MD5Code &md5, const Content &content);
};

FileOperations::FileOperations(const string &ConfigFile)
{
    Config c(ConfigFile);

    prefixPos = c.getValue("WriteRead", "prefix");
}

FileStatusCodes FileOperations::IsExists(const MD5Code &md5)
{
    
}

uint16_t FileOperations::WriteFile(const MD5Code &md5, const Content &content)
{
    if (IsExists(md5) == FILE_UNEXISTS)
        return FILE_UNEXISTS;
    
}
