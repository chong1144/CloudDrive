#pragma once
#include "Package.h"

struct DownloadReqBody
{
    char fileName[fileNameLength];
    char path[pathLength];
    char Session[SessionLength];
};
