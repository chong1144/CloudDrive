#pragma once
#include <stdint.h>
#include "Package.h"


struct UploadReqBody
{
    char fileName[fileNameLength];
    uint32_t fileSize;
    char path[pathLength];          
};
