#pragma once
#include "Package.h" 

struct SYNPushBody
{
    char Session[SessionLength];
    
    uint32_t isFile;
    uint32_t id;
    uint32_t size;
    char time[TIMELength];
    char name[nameLength];
};
