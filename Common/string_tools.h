#include <string>

#pragma once

inline void append_integer(std::string &s, uint32_t i)
{
	char snum[32];
	sprintf(snum,"%u",i);
	s += snum;
}

inline bool is_ascii_char(uint8_t c)
{
	if (c < 0x20 || c > 0x7F)
		return false;

	return true;
}

inline bool validate_is_ascii(const char* s, int max_len)
{
	for (int i = 0; i < max_len; i++)
	{
		if (s[i] == 0)
			return true;

		if (is_ascii_char(s[i]) == false)
			return false;
	}

	return true;
}


#define ERROR_LOCATION(err_msg) { err_msg += __FUNCTION__; err_msg += "() line="; append_integer(err_msg,__LINE__); err_msg += " : "; }

#ifdef ENABLE_DEBUGGING
#define DEBUG_LOCATION { if (g_debug) { fprintf(g_debug_stream, "%s() %ld\n", __FUNCTION__, __LINE__); fflush(g_debug_stream); } }

#define DEBUG_ERROR(err_msg) \
{ \
	if (g_debug) \
	{ \
		fprintf(g_debug_stream, "%s() %ld: %s\n", __FUNCTION__, __LINE__,err_msg); \
		fflush(g_debug_stream); \
	} \
} 


#define DEBUG_ERROR2(s1,s2) \
{ \
	if (g_debug) \
	{ \
		fprintf(g_debug_stream, "%s() %ld: %s%s\n", __FUNCTION__, __LINE__,s1,s2); \
		fflush(g_debug_stream); \
	} \
} 

#define DEBUG_ERROR3(s1,s2,s3) \
{ \
	if (g_debug) \
	{ \
		fprintf(g_debug_stream, "%s() %ld: %s%s%s\n", __FUNCTION__, __LINE__,s1,s2,s3); \
		fflush(g_debug_stream); \
	} \
} 


#else
#define DEBUG_LOCATION
#define DEBUG_ERROR(s) {}
#define DEBUG_ERROR2(s1,s2) {}
#define DEBUG_ERROR3(s1,s2,s3) {}
#endif

#define DEBUG_MSG(s) DEBUG_ERROR(s)
#define DEBUG_MSG2(s1,s2) DEBUG_ERROR2(s1,s2)
#define DEBUG_MSG3(s1,s2,s3) DEBUG_ERROR3(s1,s2,s3);

inline bool string_matches_spec(const char* spec, const char* s)
{
string_matches_spec_top:

	while ((*spec) == '*') spec++;

	if ((*spec) == 0)
		return true;

	while (*s != *spec)
	{
		if (*s == 0)
			return false;

		s++;
	}

	while (*s == *spec)
	{
		if (*s == 0)
			return true;

		s++;
		spec++;
	}

	if (*spec == 0)
		return false;

	if (*s == 0)
		return false;

	if (*spec == '*')
		goto string_matches_spec_top;

	return false;
}

inline int replace_string(vector<char>& code, const char* find, const char* replace)
{
	int nreplace = 0;
	int find_len = strlen(find);
	int replace_len = strlen(replace);

	int delta_len = replace_len - find_len;

	for (int i = 0; i < code.size() - find_len; i++)
	{
		if (strncmp(&code[i], find, find_len) == 0)
		{
			const char* c = &code[i];

			if (delta_len > 0)
			{
				code.insert(code.begin() + i, delta_len, 0);
			}
			else if (delta_len < 0)
			{
				code.erase(code.begin() + i, code.begin() - delta_len);
			}

			c = &code[i];

			for (int j = 0; j < replace_len; j++)
				code[i + j] = replace[j];

			i += replace_len;

			nreplace++;
		}
	}

	return nreplace;
}
