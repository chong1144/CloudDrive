#pragma once
#include "Package.h"


struct UploadReqBody
{
    char fileName[fileNameLength];
    uint32_t fileSize;
    char MD5[MD5Length];
    char path[pathLength];
    char Session[SessionLength];     
};
