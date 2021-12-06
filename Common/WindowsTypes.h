// Copyright (c) AlgoMachines
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#ifdef WIN32

#include "Windows.h"
#include "inttypes.h"

#else

typedef struct _GUID {
    uint32_t  Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[ 8 ];
} GUID;

#endif

