#pragma once

#include <string>
#include "MurmurHash3.h"

using namespace std;


// randomize_multipe - make this a large number e.g. 10000 to force a cpu intensive encryption / decription processing time
//                     by making decryption computationally expensive, the number of passwords that someone can test is limited
// This is the one function which does reversable data encryption, combining the e file with the buffer
// buffer - buffer to encrypt
// buffer_sz - size of buffer in bytes
// key - password or hash used to randomize the encryption
// key_sz - size of the key in bytes
// e - a random set of data used to encrypt the buffer
// e_sz - size of e in bytes
// randomize_interval - smaller for more obfuscation and slower speed, larger for faster but less obfuscation
void __encrypt__(uint8_t *buffer, int32_t buffer_sz, uint8_t *key, int32_t key_sz, const uint8_t *e, uint32_t e_sz, uint32_t randomize_interval=1, uint32_t randomize_multiple=1)
{
#ifdef HIST
	// Create a histogram of indexes of e[] used
	if (hist == 0)
	{
		hist = new DWORD[e_sz];
		memset(hist, 0, sizeof(DWORD)*e_sz);
	}
#endif

#if _DEBUG
	if (e == 0)
		__debugbreak();
#endif

	uint32_t idx = 0;
	uint32_t _idx = 0xFFFFFFFF;
	uint32_t seed = (uint32_t)idx;

	for (int i = 0; i < buffer_sz; i++)
	{
		if ((i%randomize_interval) == 0)
		{
			for (int j = 0; j < (int)randomize_multiple; j++)
				MurmurHash3_x86_32(key, key_sz, seed, &idx);
		}

		idx %= e_sz;

		if (idx == _idx)
		{
			idx++;
			idx %= e_sz;
		}

		_idx = idx;

#ifdef HIST
		hist[idx]++;
#endif

		buffer[i] ^= e[idx];

		key[i%key_sz] = e[idx];

		idx++;
	}
}

inline bool symmetric_encryption(uint8_t* buf, size_t buf_sz, const GUID& guid)
{
	uint8_t key[sizeof(guid)];
	const uint8_t* p = (const uint8_t*)&guid;
	
	//printf("sizeof(guid)=%ld\n",sizeof(guid));

	int i = 0;
	while (1)
	{
		key[i] = (*p);
		if (i == sizeof(guid) - 1)
			break;

		p++;
		key[i] ^= (*p);

		i = i + 1;
	}

	__encrypt__(buf, buf_sz, key, sizeof(key), (const uint8_t*)&guid, sizeof(guid));

	return true;
}

inline bool symmetric_encryption(void* b, size_t buf_sz, const void* v_key, int keysize)
{
	if (keysize < 16)
	{
		__debugbreak();
		return false;
	}

	uint32_t seed = 0;
	memmove(&seed, v_key, sizeof(seed));
	uint8_t key[16];
	MurmurHash3_x64_128(v_key, keysize, seed, key);

	__encrypt__((uint8_t*)b, (uint32_t)buf_sz, key, (uint32_t)sizeof(key), (const uint8_t*)v_key, keysize);

	return true;
}

// key - arbitrary size binary data to use to create the hash
// key_sz - size of the key
// seed - 4 byte seed
// b - buffer where random data will be written
// sz - size of buffer, will be completly filled with random data
// niter - number of iterations
// 
// Time required is about 1 or 2 seconds per GB per iteration
inline bool CreateRandomBufferFromSeed(const uint8_t *key, int key_sz, uint32_t seed, uint8_t *b, uint32_t sz, int niter=1)
{
	uint8_t *b0 = b;
	for (int iter = 0; iter < niter; iter++)
	{
		uint8_t data[16];

		// Hash the key to 16 bytes
		if (iter == 0)
		{
			MurmurHash3_x86_128(key, key_sz, seed, &data);

			if (sz < sizeof(data))
			{
				memmove(b, data, sz);
				return true;
			}
		}
		else
		{
			memmove(data, &b[sz-sizeof(data)], sizeof(data));   // Refresh data[] from the end of the last buffer
			MurmurHash3_x86_32(data, sizeof(data), seed, &seed);   // Create a new seed
			MurmurHash3_x86_128(data, sizeof(data), seed, &data);  // Refresh data[] again
		}

		// The first 16 bytes of the output buffer b are the initial hash
		memmove(b, data, sizeof(data));
		uint32_t n = sizeof(data);


		if (n != sz)
		{
			for (uint32_t i = sizeof(data); i < sz; i += sizeof(data))
			{
				// Hash the previous 16 bytes to get the next 16 bytes
				MurmurHash3_x64_128(b, sizeof(data), seed, &data);

				b += sizeof(data);

				if ((sz - n) <= sizeof(data))
				{
					memmove(b, data, (sz - n));
					break;
				}

				memmove(b, data, sizeof(data));
				n += sizeof(data);
			}
		}

		b = b0; // rewind the output buffer pointer
	}

	return true;
}

inline bool hex_char_to_bin(const string &s, vector<uint8_t> &buf) 
{
	int n = strlen(s.c_str());

	if (n % 2)
		return false; // the string must have an even number of characters

	buf.resize(n / 2);

	int idx = 0;
	for (int i = 0; i < n; i++)
	{
		int v = 0;

		if (s[i] >= '0' && s[i] <= '9')
			v = s[i] - '0';
		else if (s[i] >= 'A' && s[i] <= 'F')
			v = 10 + s[i] - 'A';
		else
			return false; // invalid character

		if (i % 2 == 0)
			buf[idx] = v << 4;
		else
		{
			buf[idx] += v;
			idx++;
		}
	}

	return true;
}

inline const char *bin_to_hex_char(const uint8_t *buf, int buf_sz, string& s)
{
	s.resize(buf_sz * 2 + 1);

	int idx = 0;
	for (int i = 0; i < buf_sz; i++)
	{
		int ic = (buf[i] & 0xF0) >> 4;
		if (ic < 0x0A)
			s[idx] = '0' + ic;
		else
			s[idx] = 'A' + ic - 10;

		idx++;

		ic = (buf[i] & 0x0F);
		if (ic < 0x0A)
			s[idx] = '0' + ic;
		else
			s[idx] = 'A' + ic - 10;

		idx++;
	}

	s[idx] = 0;

	return s.c_str();
}

inline void bin_to_ascii_char(const uint8_t* buf, int buf_sz, string& s)
{
	int bits = buf_sz * 8;

	int nchar_out = bits / 6;
	int remainder = bits % 6;
	if (remainder)
		nchar_out++;

	s.resize(nchar_out + 1);

	// 0-9 : 10 : 10
	// A-Z : 26 : 36
	// a-z : 26 : 62
	// . :  1 : 63
	// ~ :  1 : 64

	for (int i = 0; i < nchar_out; i++)
	{
		s[i] = 0;

		int v = 0;
		for (int ibit = 0; ibit < 6; ibit++)
		{
			int ibit_total = i * 6 + ibit;

			int ibyte_in = ibit_total / 8;
			int ibit_in = ibit_total % 8;

			if (ibyte_in == buf_sz)
				break;

			if ((buf[ibyte_in] & (0x01 << ibit_in)) == 0)
				continue;

			v |= 0x01 << ibit;
		}

		if (v < 10)
			s[i] = '0' + v;
		else if (v < 36)
			s[i] = 'A' + (v - 10);
		else if (v < 62)
			s[i] = 'a' + (v - 36);
		else if (v == 62)
			s[i] = '.';
		else if (v == 63)
			s[i] = '~';
		else
			__debugbreak();
	}
}

inline int get_ascii_binary_value(char c)
{
	// 0-9 : 10 : 10
	// A-Z : 26 : 36
	// a-z : 26 : 62
	// . :  1 : 63
	// ~ :  1 : 64

	if (c >= '0' && c <= '9')
		return c - '0';

	if (c >= 'A' && c <= 'Z')
		return c - 'A' + 10;

	if (c >= 'a' && c <= 'z')
		return c - 'a' + 36;

	if (c == '.')
		return 62;

	if (c == '~')
		return 63;

	return 0;
}

inline void ascii_char_to_bin(const char* s, vector<uint8_t>& buf)
{
	int len = strlen(s);

	int bits = len * 6;

	int sz = bits / 8;
	int remainder = bits % 8;
	if (remainder)
		sz++;

	buf.resize(sz);

	// 0-9 : 10 : 10
	// A-Z : 26 : 36
	// a-z : 26 : 62
	// . :  1 : 63
	// ~ :  1 : 64

	for (int ibyte = 0; ibyte < sz; ibyte++)
	{
		int v = 0;
		for (int ibit = 0; ibit < 8; ibit++)
		{
			int ibit_total = ibyte * 8 + ibit;

			int ichar = ibit_total / 6;
			int ichar_bit = ibit_total % 6;

			if (ichar == len)
			{
				buf.resize(ibyte);
				return;
			}

			int char_value = get_ascii_binary_value(s[ichar]);

			if ((char_value & (0x01 << ichar_bit)) == 0)
				continue;

			v |= 0x01 << ibit;
		}

		buf[ibyte] = v;
	}
}

inline GUID create_random_guid(void)
{
	uint64_t t = get_time_ms();

	int niter = 10 + t % 50;

	union
	{
		uint8_t buf[sizeof(GUID) * 4];
		GUID guid[4];
	};

	guid[0] = { 0x9d721eb6, 0x838, 0x4a6f, { 0xab, 0x6e, 0x39, 0x25, 0xf1, 0x7c, 0xd4, 0x7e } };
	guid[1] = { 0x3b563125, 0xfbd7, 0x4025, { 0xbb, 0x59, 0x2b, 0xca, 0xd0, 0xee, 0xe9, 0x28 } };
	guid[2] = { 0x911cd9b6, 0x266a, 0x4f7e, { 0xa1, 0x3d, 0xb2, 0x7e, 0x4d, 0xda, 0x20, 0x51 } };
	guid[3] = { 0x19b584a7, 0x741, 0x488c, { 0xa2, 0xae, 0x7b, 0xc, 0xce, 0x64, 0x83, 0x3c } };

	uint8_t* b = buf;
	memmove(b, &t, sizeof(t)); b += sizeof(GUID); t + niter;
	memmove(b, &t, sizeof(t)); b += sizeof(GUID); t + niter;
	memmove(b, &t, sizeof(t)); b += sizeof(GUID); t + niter;
	memmove(b, &t, sizeof(t));

	GUID guid_out;
	uint32_t seed = (uint32_t)(t % 0xFFFFFFFF);
	CreateRandomBufferFromSeed(buf, sizeof(buf), seed, (uint8_t*)&guid_out, sizeof(GUID), niter);

	return guid_out;
}
