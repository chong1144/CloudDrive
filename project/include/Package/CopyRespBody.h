#pragma once
#include "Package.h" 

enum CopyRespCodes
{
    COPY_SUCCESS,
    COPY_FAILED
};


struct CopyRespBody
{
    char Session[SessionLength];
    CopyRespCodes code;
};
