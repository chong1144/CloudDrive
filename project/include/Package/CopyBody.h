#pragma once
#include "Package.h" 

struct CopyBody
{
    char Session[SessionLength];
    char fileName[fileNameLength];
    char path[pathLength];
    char fileNameTo[fileNameLength];
    char pathTo[pathLength];
};
