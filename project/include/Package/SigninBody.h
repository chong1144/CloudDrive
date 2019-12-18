#pragma once
#include "Package.h"

struct SigninBody
{
    char Username[UsernameLength];
    char Password[PasswordLength];
    char IP[IPLength];
};