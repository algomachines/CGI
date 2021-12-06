// Copyright (c) AlgoMachines
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "DRM_ProgramRecord.h"

#pragma once

const bool g_debug = true;
FILE* g_debug_stream = stdout;

// will be used to collect output which will subsequently be sent to stdout with guid encryption
vector<char> g_stdout_cache;

inline void CacheStdout(const char* s1, const char* s2 = 0, const char* s3 = 0, int insert_pos = -1)
{
	if (s1 == 0)
		return;

	int len1 = strlen(s1);
	int len2 = 0, len3 = 0;
	if (s2) len2 += strlen(s2);
	if (s3) len3 += strlen(s3);

	int n = g_stdout_cache.size();
	if (n) n--;
	g_stdout_cache.resize((size_t)(n + len1 + len2 + len3 + 1));

	if (insert_pos < -1 || insert_pos > n) // if insert_position is invalid, then append
		insert_pos = -1;

	if (insert_pos == -1) // Insert at the end
	{
		memmove(&g_stdout_cache[n], s1, len1); n += len1;
		if (len2) { memmove(&g_stdout_cache[n], s2, len2); n += len2; }
		if (len3) { memmove(&g_stdout_cache[n], s3, len3); n += len3; }
		return;
	}

	char* b = &g_stdout_cache[0];
	int ninsert = len1 + len2 + len3;

	n = g_stdout_cache.size();
	n--;

	int iget = insert_pos;
	int iput = insert_pos + ninsert;

	// Move content before insert
	while (1)
	{
		b[iput++] = b[iget++];
		if (iput == n)
			break;
	}

	memmove(&g_stdout_cache[insert_pos], s1, len1); insert_pos += len1;
	if (len2) { memmove(&g_stdout_cache[insert_pos], s2, len2); insert_pos += len2; }
	if (len3) { memmove(&g_stdout_cache[insert_pos], s3, len3); insert_pos += len3; }
}

// Returns -1 if g_stdout_cache has zero size
inline int CacheGetInsertPosition(void)
{
	int sz = g_stdout_cache.size();
	return sz - 1;
}

inline void CacheBinStdout(const void* v, int sz)
{
	int n = g_stdout_cache.size();
	g_stdout_cache.resize(n + sz);
	memmove(&g_stdout_cache[n], v, sz);
}

// key is 16 bytes
// guid0 - unmodified guid
// guid1 - modified guid
inline bool create_modified_guid(const uint8_t* key, const uint8_t* guid0, uint8_t* guid1)
{
	uint32_t iter = 0, seed = 0;

	uint32_t v0, v1, shift_down, pattern;

	uint8_t data[32];
	memmove(data, key, 16);
	memmove(&data[16], guid0, 16);

	pattern = 0xC;

	while (1)
	{
		MurmurHash3_x64_128(data, 32, seed, guid1);

		memmove(&seed, guid1, sizeof(seed));

		shift_down = 30;
		v0 = seed >> shift_down;

		v1 = seed & pattern;
		shift_down = 2;
		v1 = v1 >> shift_down;

		iter = iter + 1;

		if (v0 == v1)
			break;
	}

	return true;
}

// instance_hash is 16 bytes - this value is only computed on the server
inline bool get_instance_hash(const DRM_ProgramRecord* program_rec, uint8_t* instance_hash)
{
	uint64_t n = program_rec->GetNQueries();
	vector<uint8_t> buf;
	buf.resize(56);
	memmove(&buf[0], &n, 8);
	memmove(&buf[8], program_rec->GetKey(), 16);
	memmove(&buf[24], program_rec->GetID(), 32);

	MurmurHash3_x64_128(&buf[0], buf.size(), (uint32_t)n, instance_hash);

	return true;
}

// instance_hash is 16 bytes - this value is only computed on the server
inline bool get_instance_hash_prev(const DRM_ProgramRecord* program_rec, uint8_t* instance_hash)
{
	uint64_t n = program_rec->GetNQueries();
	if (n == 0)
		return false;

	n--;

	vector<uint8_t> buf;
	buf.resize(56);
	memmove(&buf[0], &n, 8);
	memmove(&buf[8], program_rec->GetKey(), 16);
	memmove(&buf[24], program_rec->GetID(), 32);

	MurmurHash3_x64_128(&buf[0], buf.size(), (uint32_t)n, instance_hash);

	return true;
}


inline bool SendEncryptedCachedStdout(const DRM_ProgramRecord* prog_rec, bool modify_leading_guid=true)
{
	if (g_stdout_cache.size() == 0)
		return false;

	if (prog_rec == 0)
		return false;

	GUID leading_guid = create_random_guid();	

	uint8_t instance_hash[16];
	get_instance_hash(prog_rec, instance_hash);

#ifdef ENABLE_DEBUGGING
	string s;
	bin_to_ascii_char(instance_hash, 16, s);
	DEBUG_MSG2("instance_hash[out] ", s.c_str());
#endif

	if (modify_leading_guid)
	{
		GUID modified_leading_guid;
		create_modified_guid(prog_rec->GetKey(), (const uint8_t*)&leading_guid, (uint8_t*)&modified_leading_guid);
		symmetric_encryption(instance_hash, sizeof(instance_hash), modified_leading_guid);
		symmetric_encryption((uint8_t*)&g_stdout_cache[0], g_stdout_cache.size(), modified_leading_guid);
	}
	else
	{
		symmetric_encryption(instance_hash, sizeof(instance_hash), leading_guid);
		symmetric_encryption((uint8_t*)&g_stdout_cache[0], g_stdout_cache.size(), leading_guid);
	}

	string output_string;

	vector<uint8_t> buf1;
	buf1.resize(sizeof(GUID) + sizeof(instance_hash) + g_stdout_cache.size());
	uint8_t* b = &buf1[0];
	memmove(b, &leading_guid, sizeof(GUID)); b += sizeof(GUID);
	memmove(b, instance_hash, sizeof(instance_hash)); b += sizeof(instance_hash);
	memmove(b, &g_stdout_cache[0], g_stdout_cache.size());

	bin_to_hex_char(&buf1[0], buf1.size(), output_string);

	printf("{%s}", output_string.c_str());

	return true;
}