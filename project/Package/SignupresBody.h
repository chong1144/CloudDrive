#pragma once
#include "Package.h"

enum SignupCodes
{
    SIGNUP_SUCCESS,
    SIGNUP_PASSWORD_SHORT,
    SIGNUP_PASSWORD_LONG,
    SIGNUP_EXISTED_USERNAME,
    SIGNUP_USERNAME_LONG
};

struct SignupresBody
{
    char Session[SessionLength];
    SignupCodes code;
};
