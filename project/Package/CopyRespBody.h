#pragma once
#include "Package.h" 

enum CopyRespCodes
{
    COPY_SUCCESS,
    COPY_FAILED
};


struct CopyRespBody
{
    CopyRespCodes code;
    char Session[SessionLength];
};
