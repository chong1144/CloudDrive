#pragma once
#include "Package.h"

struct UploadDoneBody
{
    char MD5[MD5Length];
    bool successful;
    bool immediate;
};