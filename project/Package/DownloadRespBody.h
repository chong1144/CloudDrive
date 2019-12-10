#pragma once
#include "Package.h"

enum DownloadRespCodes
{
    DOWNLOAD_SUCCESS,
    DOWNLOAD_UNEXIST_FILE
};

struct DownloadRespBody
{
    DownloadRespCodes code;
    uint16_t chunkNum;
    char Session[SessionLength];
    char ExternInformation[ExternInformationLength];
};
