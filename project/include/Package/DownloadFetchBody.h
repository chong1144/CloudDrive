#include "Package.h"

struct DownloadFetchBody
{
    char MD5[MD5Length];
    uint16_t chunkNo;
};