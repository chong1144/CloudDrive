#pragma once

#include "Package.h"

struct FileInfoBody
{
	char md5[64];
	bool exist;		
	bool completed;	
	uint32_t size;	//chunk counts
};