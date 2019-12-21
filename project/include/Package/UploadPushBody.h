#pragma once

#include "Package.h"

//struct UploadPushBody
//{
//    char fileName[fileNameLength];
//    char path[pathLength];
//    uint16_t id;
//    char content[ChunkSize];
//    uint16_t len;
//    char Session[SessionLength];
//};

struct UploadPushBody
{
    char md5[MD5Length];
    uint16_t id; //0-1023
    char content[ContentSize];
    uint16_t len;
    bool last;
};