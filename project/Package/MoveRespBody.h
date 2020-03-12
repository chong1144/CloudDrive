#pragma once
#include "Package.h" 

enum MoveRespCodes
{
    DELETE_SUCCESS,
    DELETE_FAILED
};


struct MoveRespBody
{
    MoveRespCodes code;
    char Session[SessionLength];
};
