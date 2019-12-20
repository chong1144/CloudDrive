#pragma once
#include "Package.h"

// struct DownloadPushBody
// {
//     char fileName[fileNameLength];
//     char path[pathLength];
//     uint16_t id;
//     char content[ContentSize];
//     char Session[SessionLength];
// };

struct DownloadPushBody
{
    char md5[MD5Length];
    uint16_t id;    //0-1023    
    char content[ContentSize];
    uint16_t len;
    bool last;
};
