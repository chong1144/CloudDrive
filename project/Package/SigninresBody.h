#pragma once
#include "Package.h"

enum SigninCodes
{
    SUCCESS,
    INCORRECT_PASSWORD,
    UNEXIST_USERNAME
};


struct SigninresBody
{
    SigninCodes code;
    char Session[SessionLength];
    char ExternInformation[ExternInformationLength];
};
