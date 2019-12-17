#pragma once 
#include "Package.h"

struct SignupBody
{
    char Username[UsernameLength];
    char Password[PasswordLength];  
    char IP[IPLength];  
};
