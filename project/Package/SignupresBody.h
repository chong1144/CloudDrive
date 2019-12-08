#pragma once
#include "Package.h"

enum SignupCodes
{
    SUCCESS,
    PASSWORD_SHORT,
    PASSWORD_LONG,
    EXISTED_USERNAME,
    USERNAME_LONG
};

struct SignupresBody
{
    SignupCodes code;
    char Session[SessionLength];
    char ExternInformation[ExternInformationLength];
};
