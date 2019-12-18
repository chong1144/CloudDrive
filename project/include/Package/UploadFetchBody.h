#pragma once
#include "Package.h"


//struct UploadFetchBody
//{
//    char fileName[fileNameLength];
//    char path[pathLength];
//    uint16_t id;
//    char Session[SessionLength];
//};

struct UploadFetchBody
{
    char MD5[MD5Length];
    uint16_t chunkNo;
};
