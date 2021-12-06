// Copyright (c) AlgoMachines
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

class DRM_PrivateMessageRecord
{
public:

	static uint32_t SenderIDIndexNum(void)
	{
		return 1;
	}

	inline DRM_PrivateMessageRecord(void)
	{
		Zero();
	}

	inline void Zero(void)
	{
		ZERO(m_hashed_ID_Receiver);
		ZERO(m_hashed_ID_Sender);
		ZERO(m_Timestamp_ms);
		ZERO(m_Message);
	}

	inline bool operator < (const DRM_PrivateMessageRecord& rec) const
	{
		int state = memcmp(m_hashed_ID_Receiver, rec.m_hashed_ID_Receiver, sizeof(m_hashed_ID_Receiver));
		if (state < 0)
			return true;

		if (state > 0)
			return false;

		state = memcmp(m_hashed_ID_Sender, rec.m_hashed_ID_Sender, sizeof(m_hashed_ID_Sender));
		if (state < 0)
			return true;

		if (state > 0)
			return false;

		if (m_Timestamp_ms < rec.m_Timestamp_ms)
			return true;		
		
		if (m_Timestamp_ms > rec.m_Timestamp_ms)
			return false;

		return false; // equal
	}

	inline bool operator > (const DRM_PrivateMessageRecord& rec) const
	{
		int state = memcmp(m_hashed_ID_Receiver, rec.m_hashed_ID_Receiver, sizeof(m_hashed_ID_Receiver));
		if (state > 0)
			return true;

		if (state < 0)
			return false;

		state = memcmp(m_hashed_ID_Sender, rec.m_hashed_ID_Sender, sizeof(m_hashed_ID_Sender));
		if (state > 0)
			return true;

		if (state < 0)
			return false;

		if (m_Timestamp_ms > rec.m_Timestamp_ms)
			return true;

		if (m_Timestamp_ms < rec.m_Timestamp_ms)
			return false;

		return false; // equal
	}

	inline bool HasSameData(const DRM_PrivateMessageRecord& rec) const
	{
		if (m_Timestamp_ms != rec.m_Timestamp_ms)
			return false;

		if (memcmp(m_Message, rec.m_Message, sizeof(m_Message)))
			return false;

		return true;
	}

	inline bool Assign(const DRM_PrivateMessageRecord& rec, string& err_msg)
	{
		memmove(this, &rec, GetSizeBytes());
		return true;
	}

	inline uint32_t LoadFromBuffer(const void* b)
	{
		memmove(this, b, GetSizeBytes());
		return GetSizeBytes();
	}

	inline bool Update(const DRM_PrivateMessageRecord& rec, string& err_msg) const
	{
		m_Timestamp_ms = rec.m_Timestamp_ms;
		memmove(m_Message, rec.m_Message, sizeof(m_Message));
		return true;
	}

	inline void SetHashedIDSender(const void* buf)
	{
		memmove(m_hashed_ID_Sender, buf, sizeof(m_hashed_ID_Sender));
	}

	inline const void* GetHashedIDSender(void) const
	{
		return m_hashed_ID_Sender;
	}

	inline void SetHashedIDReceiver(const void* buf)
	{
		memmove(m_hashed_ID_Receiver, buf, sizeof(m_hashed_ID_Receiver));
	}

	inline const void* GetHashedIDReciever(void) const
	{
		return m_hashed_ID_Receiver;
	}

	inline void SetTimestamp(const void* buf)
	{
		memmove(&m_Timestamp_ms, buf, sizeof(m_Timestamp_ms));
	}

	inline uint64_t GetTimestamp(void) const
	{
		return m_Timestamp_ms;
	}

	inline void SetMessage(const uint8_t* buf, uint16_t len=0)
	{
		ZERO(m_Message);
		uint32_t i = 0;
		while (i < sizeof(m_Message))
		{
			m_Message[i] = buf[i];
			if (buf[i] == 0)
				break;
			i++;

			if (len && i == len)
				break;
		}
	}

	inline char get_ascii_char(uint8_t v) const 
	{
		if (v >= 0x20 && v <= 0x127)
			return v;
		
		if (v == '\n')
			return v;

		return '_';
	}

	// return the strlen of s
	inline int GetMessage(string &s) const
	{
		char msg[257];
		ZERO(msg);
		for (int i = 0; i < sizeof(m_Message); i++)
		{
			if (m_Message[i] == 0)
			{
				s = msg;
				return i;
			}

			msg[i] = get_ascii_char(m_Message[i]);
		}

		s = msg;

		return 256;
	}

	static uint32_t GetSizeBytes(void)
	{
		return 32 + 32 + 8 + 256;
	}

	inline const char* Report(string& s) const
	{
		string tmp;
		bin_to_ascii_char(m_hashed_ID_Sender, sizeof(m_hashed_ID_Sender), tmp);
		s += "hashed_ID_Sender: ";
		s += tmp.c_str();
		s += "\n";
		bin_to_ascii_char(m_hashed_ID_Receiver, sizeof(m_hashed_ID_Receiver), tmp);
		s += "m_hashed_ID_Receiver: ";
		s += tmp.c_str();
		s += "\n";

		char tmp1[128];
		sprintf(tmp1, "Timestamp_ms: %llu\n", m_Timestamp_ms);
		s += tmp1;

		s += (const char*)m_Message;

		return s.c_str();
	}

private:

	uint8_t m_hashed_ID_Receiver[32];
	uint8_t m_hashed_ID_Sender[32];
	mutable uint64_t m_Timestamp_ms;
	mutable uint8_t m_Message[256];
};
