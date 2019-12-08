#pragma once
#include "Package.h"


struct UploadFetchBody
{
    char fileName[fileNameLength];
    char path[pathLength];
    uint16_t id;
    char Session[SessionLength];
};
