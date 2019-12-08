#pragma once
#include "Package.h"

struct DownloadPushBody
{
    char fileName[fileNameLength];
    char path[pathLength];
    uint16_t id;
    char content[ChunkSize];
    char Session[SessionLength];
};
