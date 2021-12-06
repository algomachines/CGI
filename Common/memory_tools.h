#include "memory.h"

#pragma once

#define ZERO(b) { memset(&b,0,sizeof(b)); }

#define LFB(X) { memmove(&X, sizeof(X), b);  b += sizeof(X); sz += sizeof(X); }


