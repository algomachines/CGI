// Copyright (c) AlgoMachines
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#define ID_SIZE_BYTES 32

inline void hash_id(uint8_t* ID)
{
	if (ID_SIZE_BYTES == 32)
	{
		uint8_t ID_hashed[ID_SIZE_BYTES];

		uint32_t seed;
		memmove(&seed, ID, sizeof(seed));
		MurmurHash3_x86_128(ID, ID_SIZE_BYTES, seed, &ID_hashed[0]);
		memmove(&seed, ID_hashed, sizeof(seed));
		MurmurHash3_x86_128(ID, ID_SIZE_BYTES, seed, &ID_hashed[16]);

		memmove(ID, ID_hashed, ID_SIZE_BYTES);
	}
	else
		__debugbreak();
}

class DRM_ProgramRecord
{
public:

	inline DRM_ProgramRecord(void)
	{
		Zero();
	}

	inline void Zero(void)
	{
		ZERO(m_ID);
		m_TimeLastQuery_ms = 0;
		m_ExpirationTime_ms = 0;
		m_NQueries = 0;
		m_MaxNQueries = 0;
		ZERO(m_key);
	}

	static uint32_t GetSizeBytes(void)
	{
		return sizeof(DRM_ProgramRecord);
	}

	inline bool IsEqualTo(const DRM_ProgramRecord& rec)
	{
		if (memcmp(m_ID, rec.m_ID, sizeof(m_ID))) return false;
		if (m_TimeLastQuery_ms != rec.m_TimeLastQuery_ms) return false;
		if (m_ExpirationTime_ms != rec.m_ExpirationTime_ms) return false;
		if (m_NQueries != rec.m_NQueries) return false;
		if (m_MaxNQueries != rec.m_MaxNQueries) return false;
		if (memcmp(m_key, rec.m_key, sizeof(m_key))) return false;
		return true;
	}

	inline bool Update(const DRM_ProgramRecord& rec, string& err_msg) const
	{
		m_TimeLastQuery_ms = rec.m_TimeLastQuery_ms;
		m_ExpirationTime_ms = rec.m_ExpirationTime_ms;
		m_NQueries = rec.m_NQueries;
		m_MaxNQueries = rec.m_MaxNQueries;
		memmove(m_key, rec.m_key, sizeof(m_key));
		return true;
	}

	inline const bool operator < (const DRM_ProgramRecord& rec) const
	{
		if (memcmp(m_ID, rec.m_ID, sizeof(m_ID)) < 0)
			return true;

		return false;
	}

	inline const bool operator > (const DRM_ProgramRecord& rec) const
	{
		if (memcmp(m_ID, rec.m_ID, sizeof(m_ID)) > 0)
			return true;

		return false;
	}

	inline uint32_t LoadFromBuffer(const void* buf)
	{
		memmove(this, buf, sizeof(DRM_ProgramRecord));
		return sizeof(DRM_ProgramRecord);
	}

	inline bool Assign(const DRM_ProgramRecord& rec, string& err_msg)
	{
		memmove(this, &rec, sizeof(DRM_ProgramRecord));
		return true;
	}

	inline bool HasSameData(const DRM_ProgramRecord& rec)
	{
		if (memcmp(this, &rec, sizeof(DRM_ProgramRecord)))
			return false;

		return true;
	}

	inline void SetTimeLastQuery_ms(uint64_t t)
	{
		m_TimeLastQuery_ms = t;
	}

	inline void SetExpirationTime_ms(uint64_t t)
	{
		m_ExpirationTime_ms = t;
	}

	inline void SetNQueries(uint64_t i)
	{
		m_NQueries = i;
	}

	inline void SetMaxNQueries(uint64_t n)
	{
		m_MaxNQueries = n;
	}

	inline uint64_t GetTimeLastQuery_ms(void) const
	{
		return m_TimeLastQuery_ms;
	}

	inline uint64_t GetExpirationTime_ms(void) const
	{
		return m_ExpirationTime_ms;
	}

	inline uint64_t GetNQueries(void) const
	{
		return m_NQueries;
	}

	inline bool IncrementNQueries(void) const
	{
		m_NQueries++;

		if (m_MaxNQueries && (m_NQueries > m_MaxNQueries))
			return false;

		return true;
	}

	inline uint64_t GetMaxNQueries(void) const
	{
		return m_MaxNQueries;
	}

	inline const uint8_t* GetID(void) const
	{
		return m_ID;
	}

	inline uint8_t* GetIDWritable(void)
	{
		return m_ID;
	}

	inline void SetID(const uint8_t* id)
	{
		memmove(m_ID, id, sizeof(m_ID));
	}

	inline const uint8_t *GetKey(void) const
	{
		return m_key;
	}

	inline void SetKey(const uint8_t* key) const
	{
		memmove(m_key, key, sizeof(m_key));
	}

private:

	uint8_t m_ID[ID_SIZE_BYTES];			// 32 byte program ID - this is the index
	mutable uint64_t m_TimeLastQuery_ms;	// Time of the last query in ms since Jan 1, 1970
	mutable uint64_t m_ExpirationTime_ms;	// Experiation time for this ID in ms since Jan 1, 1970
	mutable uint64_t m_NQueries;			// Number of queries made so far
	mutable int64_t m_MaxNQueries;			// Maximum number of queries allowed
	mutable uint8_t m_key[16];				// Encryption key used to modify the guid during communication
};

