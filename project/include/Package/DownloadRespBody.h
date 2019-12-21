#pragma once
#include "Package.h"

const char DOWNLOAD_FILE_EXIST = 0x01;
const char DOWNLOAD_FILE_NOTEXIST = 0x00;
struct DownloadRespBody
{

    // enum DownloadRespCodes
    // {
    //     DOWNLOAD_SUCCESS,
    //     DOWNLOAD_UNEXIST_FILE
    // };
    char state;
    uint16_t chunkNum;
    char ExternInformation[ExternInformationLength];
};
