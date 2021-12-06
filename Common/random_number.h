// Copyright (c) AlgoMachines
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "MurmurHash3.h"
#include "time_tools.h"

#pragma once

#ifdef WINDOWS

inline bool randomize_buffer(uint8_t* b, int32_t sz)
{
	uint32_t seed;	
	if (sz < sizeof(seed)) 
		return false;

	memmove(&seed, b, sizeof(seed));

	// Start with the systme state hash and iteratively hash it to get the random data
	uint8_t data[32], data1[32];
	
	for (int i=0; i < sizeof(data); i++) data[i] = b[i%sz]; // the initial state of data is the beginning of the buffer

	int n = 0;
	while (sz > 0)
	{
		MurmurHash3_x64_128(data, 32, seed, &data1[0]);      // new random data for the first  16 bytes of data[]
		MurmurHash3_x64_128(data, 16, seed, &data1[16]);		// new random data for the second 16 bytes of data[]

		memmove(&seed, data, sizeof(seed)); // randomize seed

		memmove(data, data1, 32);

		memmove(b, data, min(32, sz));

		b += 32;
		sz -= 32;
	}

	return true;
}

// time_target_ms - this function will continue to run for this number of ms - continuously randomizing the buffer
inline void create_random_buffer(uint8_t* b, int32_t sz, uint32_t time_target_ms=0)
{
	uint8_t* b0 = b; 

	SYSTEMTIME st;
	GetSystemTime(&st);

	FILETIME ft;

	SystemTimeToFileTime(&st, &ft);

	uint32_t seed;
	MurmurHash3_x86_32(&ft, sizeof(ft), 0, &seed);

	uint8_t seed_system_state[32];

	// Gather computer/state specific info and create a hash
	char computer_name[1024];
	DWORD len = sizeof(computer_name);
	GetComputerName(computer_name, &len);

	ULARGE_INTEGER  total;
	BOOL status = ::GetDiskFreeSpaceEx(NULL, NULL, &total, NULL);

	MEMORYSTATUSEX mem;
	mem.dwLength = sizeof(mem);
	status = GlobalMemoryStatusEx(&mem);
	DWORD err = GetLastError();

	int buf_sz = len + sizeof(total) + sizeof(mem);

	// Allocate some memory
	vector<char> tmp;
	tmp.resize(buf_sz);
	char* buf = &tmp[0];

	// Move the computer specific information into buf
	int i = 0;
	memmove(buf + i, computer_name, len); i += len;
	memmove(buf + i, &total, sizeof(total)); i += sizeof(total);
	memmove(buf + i, &mem, sizeof(mem)); i += sizeof(mem);

	// Hash buf to get the system_state_seed[]
	MurmurHash3_x64_128(buf, sizeof(buf_sz), (UINT)(total.LowPart), &seed_system_state[0]);
	MurmurHash3_x64_128(buf, sizeof(buf_sz), seed, &seed_system_state[16]);

	// Start with the system state hash and iteratively hash it to get the random data
	int sz0 = sz;
	uint8_t data[32];
	while(sz > 0)
	{
		memmove(data, seed_system_state, 32);
		MurmurHash3_x64_128(seed_system_state, 32, seed, &data[0]);      // new random data for the first  16 bytes of data[]
		memmove(seed_system_state, data, 16);
		MurmurHash3_x64_128(seed_system_state, 16, seed, &data[16]);		// new random data for the second 16 bytes of data[]
		memmove(seed_system_state, data, 32);

		memmove(&seed, data, sizeof(seed)); // randomize seed

		memmove(b, data, min(32,sz));

		b += 32;
		sz -= 32;
	}

	if (time_target_ms == 0) // no time based randomization requested
		return;

	// continue to randomize the buffer for the designated number of ms

	b = b0; // restore the address of the buffer which is being processed

	if (sz0 < 4)
		return; // randomize_buffer does nothing unless the buffer size is at least 4 bytes

	sz = sz0; 
	uint64_t t0 = get_time_ms();
	uint64_t t1 = t0;
	uint64_t ntotal = 0;
	while (t1 - t0 < time_target_ms)
	{
		uint32_t n = (uint32_t)b[0];
		n++;

		ntotal += n;

		while (n) // between 1 and 256 times through this loop each time
		{
			randomize_buffer(b, sz);
			n--;
		}

		t1 = get_time_ms(); // grab the time again so that we can check to see if full time has expired
	}
}

#endif
