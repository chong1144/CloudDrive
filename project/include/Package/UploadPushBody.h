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
    uint16_t id; //0-1023
    char content[ChunkSize];
    uint16_t len;
    bool last;
};