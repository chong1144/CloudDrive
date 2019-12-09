#pragma once
#include "Package.h"

enum SigninCodes
{
    SIGNIN_SUCCESS,
    SIGNIN_INCORRECT_PASSWORD,
    SIGNIN_UNEXIST_USERNAME
};


struct SigninresBody
{
    SigninCodes code;
    char Session[SessionLength];
    char ExternInformation[ExternInformationLength];
};
