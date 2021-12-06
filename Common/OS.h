// Copyright (c) AlgoMachines
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#ifdef WINDOWS
#include "Windows.h"
#define WIN32
#endif

#include <vector>
#include "string.h"

using namespace std;

inline bool IsZero(const vector<uint8_t>& b, uint32_t offset = 0)
{
	for (int i = offset; i < b.size(); i++)
	{
		if (b[i])
			return false;
	}

	return true;
}
