#pragma once
#include "Package.h"



enum MkdirRespCodes
{
    MKDIR_SUCCESS,
    MKDIR_ALREADY_HAS
};

struct MkdirRespBody
{
    MkdirRespCodes code;
    char Session[SessionLength];
    char ExternInformation[ExternInformationLength];
};
