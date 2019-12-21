#pragma once

#include <stdint.h>

const uint16_t UsernameLength = 256;
const uint16_t PasswordLength = 256;
const uint16_t SessionLength = 256;
const uint16_t ExternInformationLength = 256;
const uint16_t IPLength = 32;

const uint16_t pathLength = 256;
const uint16_t fileNameLength = 256;
const uint32_t ChunkSize = 1024*1024;
const uint32_t ContentSize = 1024;
const uint16_t nameLength = 256;
const uint16_t MD5Length = 64;

#include "SigninBody.h"
#include "SigninresBody.h"

#include "SignupBody.h"
#include "SignupresBody.h"

#include "UploadReqBody.h"
#include "UploadRespBody.h"
#include "UploadFetchBody.h"
#include "UploadPushBody.h"
#include "UploadDone.h"

#include "DownloadReqBody.h"
#include "DownloadRespBody.h"
#include "DownloadPushBody.h"
#include "DownloadFetchBody.h"
#include "DownloadDoneBody.h"
#include "SYNReqBody.h"
#include "SYNRespBody.h" 
#include "SYNPushBody.h"

#include "MoveBody.h"
#include "MoveRespBody.h"
#include "CopyBody.h"
#include "CopyRespBody.h"
#include "DeleteBody.h"
#include "DeleteRespBody.h"
#include "FileInfoBody.h"

#include "MkdirBody.h"
#include "MkdirRespBody.h"

#include "UniformHeader.h"
