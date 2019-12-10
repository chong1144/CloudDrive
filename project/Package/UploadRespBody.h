#pragma once
#include "Package.h"

enum UploadRespCodes
{
    UPLOAD_SUCCESS,
    UPLOAD_ALREADY_HAS,
    UPLOAD_TO_LARGE
};

struct UploadRespBody
{
    UploadRespCodes code;
    uint16_t chunkNum;
    char Session[SessionLength];
    char ExternInformation[ExternInformationLength];
};
