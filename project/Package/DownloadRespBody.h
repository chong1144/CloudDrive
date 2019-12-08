#pragma once
#include "Package.h"

enum DownloadRespCodes
{
    SUCCESS,
    UNEXIST_FILE
};

struct DownloadRespBody
{
    DownloadRespCodes code;
    uint16_t chunkNum;
    char Session[SessionLength];
    char ExternInformation[ExternInformationLength];
};
