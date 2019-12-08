#pragma once
#include "Package.h"

enum SYNRespCodes
{
    SUCCESS,
    UNEXIST_DIR,
};

struct SYNRespBody
{
    SYNRespCodes code;
    uint16_t childNum;
    char Session[SessionLength];
    char ExternInformation[ExternInformationLength];
};
