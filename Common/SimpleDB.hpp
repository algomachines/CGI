// Copyright (c) AlgoMachines
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "string_tools.h"
#include "file_tools.h"

#pragma once

template <class RECORD_CLASS, class INDEX_TYPE=uint32_t> class SimpleDB 
{
private:
	vector<RECORD_CLASS> m_records;
	
	inline INDEX_TYPE linear_search(const RECORD_CLASS &record, INDEX_TYPE istart, INDEX_TYPE iend, bool &exists) const
	{	
		INDEX_TYPE i = istart;
		while (i <= iend)
		{
			if (record < m_records[i])
			{
				exists = false;
				return i;
			}
			
			if (record > m_records[i])
			{
				i++;
				continue;
			}
			
			exists = true;
			return i;
		}
		
		exists = false;
		return i; // i == iend + 1
	}
		
public:

	inline INDEX_TYPE GetNumRecords(void) const
	{
		return (INDEX_TYPE)m_records.size();
	}

	inline bool RemoveRecord(INDEX_TYPE idx, string& err_msg)
	{
		if (idx < 0 || idx >= m_records.size())
		{
			ERROR_LOCATION(err_msg);
			err_msg += "() invalid record index ";
			append_integer(err_msg, idx);
			return false;
		}

		m_records.erase(m_records.begin() + idx);

		return true;
	}

	inline bool LoadFromBuffer (const void *buffer, INDEX_TYPE nrecords, string &err_msg)
	{
		m_records.resize(nrecords);
		
		const uint8_t *b = (const uint8_t *)buffer;
		
		for (INDEX_TYPE irec=0; irec < nrecords; irec++)
		{
			INDEX_TYPE nbytes = m_records[irec].LoadFromBuffer (b);
			if (nbytes == 0)
			{
				ERROR_LOCATION(err_msg);
				err_msg += "() problem reading buffer at record ";
				append_integer(err_msg,irec);
				return false;
			}
			b += nbytes;
		}
		
		return true;
	}

	inline const RECORD_CLASS* GetRecord(const RECORD_CLASS& token, INDEX_TYPE *ret_idx=0) const
	{
		bool exists;
		INDEX_TYPE idx = GetRecordIndex(token, exists);

		if (ret_idx)
			*ret_idx = idx;

		if (exists == false)
			return 0;

		return &m_records[idx];
	}

	inline const RECORD_CLASS *GetRecordByIndex(INDEX_TYPE idx) const
	{
		if (idx < 0 || idx >= m_records.size())
			return 0;

		return &m_records[idx];
	}
	
	// Returns the index of the existing record
	// Otherwise returns the index of the insert position for this record
	inline INDEX_TYPE GetRecordIndex(const RECORD_CLASS &record, bool &exists) const
	{
		if (m_records.size() == 0)
		{
			exists = false;
			return 0;
		}
		
		// Straight search when there <= 6 records
		if (m_records.size() <= 6)
			return linear_search(record,0,m_records.size()-1, exists);
		
		INDEX_TYPE istart = 0;
		INDEX_TYPE iend = m_records.size() - 1;
		INDEX_TYPE i = m_records.size() / 2;

		while (1)
		{
			if (record < m_records[i])
				iend = i - 1;
			else
			{
				if (record > m_records[i])
					istart = i + 1;
				else
				{
					exists = true;
					return i;
				}
			}

			if (iend - istart < 4)
				return linear_search(record,istart,iend,exists);

			i = (iend - istart) / 2;
			i += istart;
		}
	}
	
	inline bool InsertRecord(const RECORD_CLASS &record, INDEX_TYPE idx, string &err_msg)
	{
		if (idx > (INDEX_TYPE)m_records.size() || idx < 0)
		{
			ERROR_LOCATION(err_msg);
			err_msg += "invalid idx: ";
			append_integer(err_msg,idx);
			return false;
		}
		
		m_records.resize (m_records.size() + 1);
		for (INDEX_TYPE i=(INDEX_TYPE)(m_records.size()-1); i > idx; i--)
		{
			if (m_records[i].Assign(m_records[i-1],err_msg) == false)
				return false;
		}
		
		return m_records[idx].Assign(record,err_msg);
	}
	
	inline bool UpdateRecord(const RECORD_CLASS &record, bool &changes_made, string &err_msg)
	{
		changes_made = false;
		
		bool exists;
		INDEX_TYPE idx = GetRecordIndex(record, exists);

		if (exists == false)
		{
			bool status = InsertRecord(record,idx,err_msg);
			if (status == true) changes_made = true;
			return status;
		}
		
		if (m_records[idx].HasSameData(record) == true)
		{
			changes_made = false;
			return true;
		}
		
		bool status = m_records[idx].Update(record, err_msg);
		if (status == true)
			changes_made = true;
			
		return status;
	}

	inline bool GenerateReport(const char* file_name, string& err_msg)
	{
		FILE* stream = fopen(file_name, "w");
		if (stream == 0)
		{
			ERROR_LOCATION(err_msg);
			err_msg += "unable to create file: ";
			err_msg += file_name;
			return false;
		}

		int i = 0;
		while (i < (int)m_records.size())
		{
			fprintf(stream, "[%ld]\n",i);

			string s;
			m_records[i].Report(s);
			fprintf(stream, "%s\n", s.c_str());

			i++;
		}

		fclose(stream);
		return true;
	}
	
	inline bool SaveToFile(const char *file_name, string &err_msg)
	{
		string unique_file_name;
		make_unique_filename(unique_file_name, file_name);
				
		FILE* stream = fopen(unique_file_name.c_str(), "wb");
		
		if (stream == 0)
		{
			ERROR_LOCATION(err_msg);
			err_msg += "unable to create file: ";
			err_msg += unique_file_name.c_str();
			return false;
		}
		
		if (m_records.size())
		{
			if (fwrite(&m_records[0], RECORD_CLASS::GetSizeBytes(), m_records.size(), stream) != m_records.size())
			{
				fclose(stream);
				ERROR_LOCATION(err_msg);
				err_msg += "problem writing data to create file: ";
				err_msg += unique_file_name.c_str();
				return false;
			}
		}

		fclose(stream);
		
		// Delete the existing db file
		if (DoesFileExist(file_name))
		{
			if (DeleteFile(file_name)==false)
			{
				ERROR_LOCATION(err_msg);
				err_msg += "unable to delete file: ";
				err_msg += file_name;
				return false;
			}
		}
		
		// Swap in the newly written db file
		if (rename(unique_file_name.c_str(),file_name))
		{
			ERROR_LOCATION(err_msg);
			err_msg += "unable to rename: ";
			err_msg += unique_file_name.c_str();
			err_msg += " -> ";
			err_msg += file_name;
			return false;
		}
		
		return true;
	}
	
	inline bool LoadFromFile(const char *file_name, string &err_msg)
	{
		int flen = filelength(file_name);
		if (flen < 0)
		{
			ERROR_LOCATION(err_msg);
			err_msg += "file does not exist: ";
			err_msg += file_name;
			return false;
		}
		
		if (flen == 0)
		{
			m_records.resize(0);
			return true;
		}
		
		INDEX_TYPE record_sz = RECORD_CLASS::GetSizeBytes();
		
		if (flen % record_sz)
		{
			ERROR_LOCATION(err_msg);
			err_msg += "file has invalid size:";
			append_integer(err_msg,flen);
			err_msg += " must be a multiple or record size: ";
			append_integer(err_msg,record_sz);
			err_msg += " :";
			err_msg += file_name;
			return false;
		}
		
		vector<uint8_t> data;
		data.resize(flen);
		
		FILE* stream = fopen(file_name, "rb");
		if (!stream)
		{
			ERROR_LOCATION(err_msg);
			err_msg += "unable to open file for reading: ";
			err_msg += file_name;
			return false;
		}
		
		if (fread(&data[0],1,flen,stream)!= flen)
		{
			fclose(stream);
			ERROR_LOCATION(err_msg);
			err_msg += "problem reading data from file: ";
			err_msg += file_name;
			return false;
		}
		
		fclose (stream);
		
		return LoadFromBuffer (&data[0],flen/record_sz,err_msg);
	}
};
