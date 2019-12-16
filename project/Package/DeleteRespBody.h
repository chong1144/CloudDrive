#pragma once
#include "Package.h" 

enum DeleteRespCodes
{
    DELETE_SUCCESS,
    DELETE_FAILED
};


struct DeleteRespBody
{
    char Session[SessionLength];
    DeleteRespCodes code;
};
