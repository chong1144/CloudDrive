#pragma once
#include "Package.h"



enum MkdirRespCodes
{
    MKDIR_SUCCESS,
    MKDIR_ALREADY_HAS
};

struct MkdirRespBody
{
    char Session[SessionLength];
    MkdirRespCodes code;
    char ExternInformation[ExternInformationLength];
};
