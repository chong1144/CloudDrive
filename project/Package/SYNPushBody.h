#pragma once
#include "Package.h" 

struct SYNPushBody
{
    char Session[SessionLength];
    bool isFile;
    char name[nameLength];
    uint16_t id;
};
