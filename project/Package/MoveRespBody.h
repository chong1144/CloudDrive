#pragma once
#include "Package.h" 

enum MoveRespCodes
{
    MOVE_SUCCESS,
    MOVE_FAILED
};


struct MoveRespBody
{
    MoveRespCodes code;
    char Session[SessionLength];
};
