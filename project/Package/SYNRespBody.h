#pragma once
#include "Package.h"

enum SYNRespCodes
{
    SYN_SUCCESS,
    SYN_UNEXIST_DIR,
};

struct SYNRespBody
{
    char Session[SessionLength];
    SYNRespCodes code;
    uint16_t childNum;
};
