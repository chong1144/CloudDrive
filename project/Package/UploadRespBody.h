#pragma once
#include "Package.h"

enum UploadRespCodes
{
    SUCCESS,
    ALREADY_HAS,
    TO_LARGE
};

struct UploadRespBody
{
    UploadRespCodes code;
    uint16_t chunkNum;
    char Session[SessionLength];
    char ExternInformation[ExternInformationLength];
};
