#pragma once
#include "Package.h"

enum SYNRespCodes
{
    SYN_SUCCESS,
    SYN_UNEXIST_DIR,
};

struct SYNRespBody
{
    SYNRespCodes code;
    uint16_t childNum;
    char Session[SessionLength];
    char ExternInformation[ExternInformationLength];
};
