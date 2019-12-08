#pragma once

#include <stdint.h>

#include "SigninBody.h"
#include "SigninresBody.h"

#include "SignupBody.h"
#include "SignupresBody.h"

#include "UploadReqBody.h"
#include "UploadRespBody.h"
#include "UploadFetchBody.h"
#include "UploadPushBody.h"

#include "DownloadReqBody.h"
#include "DownloadRespBody.h"
#include "DownloadPushBody.h"

#include "SYNReqBody.h"
#include "SYNRespBody.h" 
#include "SYNPushBody.h"


#include "UniformHeader.h"

const uint16_t UsernameLength = 256;
const uint16_t PasswordLength = 256;
const uint16_t SessionLength = 256;
const uint16_t ExternInformationLength = 256;


const uint16_t pathLength = 256;
const uint16_t fileNameLength = 256;
const uint16_t ChunkSize = 1024;
const uint16_t nameLength = 256;
