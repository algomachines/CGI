#pragma once

/*
Record structure for the transfer database :

ID of current owner - 32 byte program ID
Expiration time for offer - 64 bit unix time in ms
Allowed tries - 8 bit integer
Name - will be shown to the prospective new owner when they are prompted for the password. 32 characters null terminated.
Password - the prospective new owner will be prompted for this, 32 characters null terminated.

[ID of current owner][Expiration time for offer][Allowed tries][Name][Password]  
*/

class DRM_TransferRecord
{
public:

	inline DRM_TransferRecord(void)
	{
		Zero();
	}

	inline void Zero(void)
	{
		ZERO(m_ID[ID_SIZE_BYTES]);
		ZERO(m_ID_unhashed[ID_SIZE_BYTES]);
		m_ExpireTimeForOffer_ms = 0;
		m_AllowedTries = 0;
		ZERO(m_Name);
		ZERO(m_Password);
	}
	
	static uint32_t GetSizeBytes(void)
	{
		return sizeof(DRM_TransferRecord);
	}

	inline bool IsEqualTo(const DRM_TransferRecord& rec) const
	{
		if (memcmp(m_ID, rec.m_ID, sizeof(m_ID))) return false;
		if (m_ExpireTimeForOffer_ms != rec.m_ExpireTimeForOffer_ms) return false;
		if (m_AllowedTries != rec.m_AllowedTries) return false;
		if (strcmp(m_Name, rec.m_Name)) return false;
		if (strcmp(m_Password, rec.m_Password)) return false;
		return true;
	}

	inline bool Update(const DRM_TransferRecord& rec, string& err_msg) const
	{
		m_ExpireTimeForOffer_ms = rec.m_ExpireTimeForOffer_ms;
		m_AllowedTries = rec.m_AllowedTries;
		memmove(m_Name, rec.m_Name, sizeof(m_Name));
		memmove(m_Password, rec.m_Password, sizeof(m_Name));
		return true;
	}

	inline const bool operator < (const DRM_TransferRecord& rec) const
	{
		if (memcmp(m_ID, rec.m_ID, sizeof(m_ID)) < 0)
			return true;

		return false;
	}

	inline const bool operator > (const DRM_TransferRecord& rec) const
	{
		if (memcmp(m_ID, rec.m_ID, sizeof(m_ID)) > 0)
			return true;

		return false;
	}

	inline uint32_t LoadFromBuffer(const void* buf)
	{
		memmove(this, buf, sizeof(DRM_TransferRecord));
		return sizeof(DRM_TransferRecord);
	}

	inline bool Assign(const DRM_TransferRecord& rec, string& err_msg)
	{
		memmove(this, &rec, sizeof(DRM_TransferRecord));
		return true;
	}

	inline bool HasSameData(const DRM_TransferRecord& rec) const
	{
		if (memcmp(this, &rec, sizeof(DRM_TransferRecord)))
			return false;

		return true;
	}

	inline void SetID(const uint8_t* id)
	{
		memmove(m_ID, id, sizeof(m_ID));
	}

	inline void GetID_unhashed(uint8_t* ID) const
	{
		memmove(ID, m_ID_unhashed, ID_SIZE_BYTES);
	}

	inline void HashAndAssignID(const uint8_t* id_unhashed)
	{
		memmove(m_ID_unhashed, id_unhashed, ID_SIZE_BYTES);
		memmove(m_ID, id_unhashed, ID_SIZE_BYTES);
		hash_id(m_ID);
		hash_id(m_ID);
	}

	inline void SetExpirationTimeForOffer_ms(uint64_t v) const
	{
		m_ExpireTimeForOffer_ms = v;
	}

	inline void SetAllowedTries(uint8_t v) const
	{
		m_AllowedTries = v;
	}

	inline void SetName(const char* s) const
	{
		ZERO(m_Name);
		for (int i = 0; i < 32; i++)
		{
			if (!s[i])
				break;

			m_Name[i] = s[i];
		}
	}

	inline void SetPassword(const char* s) const
	{
		ZERO(m_Password);
		for (int i = 0; i < 32; i++)
		{
			if (!s[i])
				break;

			m_Password[i] = s[i];
		}
	}

	inline bool MatchesNameSpec(const char* spec) const
	{
		string s_name;
		GetName(s_name);

		return string_matches_spec(spec, s_name.c_str());
	}

	inline const char* GetIDAsString(string& s) const
	{
		bin_to_ascii_char(m_ID, sizeof(m_ID), s);
		return s.c_str();
	}
	
	inline const char* GetName(string &s) const
	{
		// Count up to the first zero or invalid char
		int i;
		for (i = 0; i < sizeof(m_Name); i++)
		{
			if (m_Name[i] == 0)
				break;

			if (is_ascii_char(m_Name[i]) == false)
				break;
		}

		s.resize(i + 1);

		memmove(&s[0], m_Name, i);

		return s.c_str();
	}

private:	
	
	uint8_t m_ID[ID_SIZE_BYTES];
	uint8_t m_ID_unhashed[ID_SIZE_BYTES];
	mutable uint64_t m_ExpireTimeForOffer_ms;
	mutable uint64_t m_AllowedTries;
	mutable char m_Name[32];
	mutable char m_Password[32];
};
