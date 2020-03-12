#pragma once
#include "Package.h" 

enum DeleteRespCodes
{
    DELETE_SUCCESS,
    DELETE_FAILED
};


struct DeleteRespBody
{
    DeleteRespCodes code;
    char Session[SessionLength];
};
