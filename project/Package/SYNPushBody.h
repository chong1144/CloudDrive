#pragma once
#include "Package.h" 

struct SYNPushBody
{
    bool isFile;
    char name[nameLength];
    uint16_t id;
    char Session[SessionLength];
};
