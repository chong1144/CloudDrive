#pragma once
#include "Package.h" 

struct SYNPushBody
{
    char Session[SessionLength];
    
    uint32_t isFile;
    uint32_t id;
    
    char name[nameLength];
};
