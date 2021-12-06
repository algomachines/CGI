///////////////////////////////////////////////////////////////////
// WARNING - this implementation has not been tested at ALL ///////
///////////////////////////////////////////////////////////////////

#include "string_tools.h"
#include "file_tools.h"

#pragma once

// The RECORD_CLASS defines the number of indexes and what they are.
template <class RECORD_CLASS, class INDEX_TYPE=uint32_t> class IndexedDB 
{
private:
	vector<RECORD_CLASS> m_records;  // Records are not sorted

	vector<vector<INDEX_TYPE>> m_indexes; // These indexes are maintained as records and added and removed
	// NOTE: There are no restrictions on redundant records being added here. 
	//       A record with an identical index value is added, the new record index is inserted below the existing
	//       record index.

	inline bool IsLessThan(const RECORD_CLASS& record, const INDEX_TYPE& index_num, const INDEX_TYPE& idx) const
	{
		return record.IsLessThan(m_records[m_indexes[index_num][idx]]);
	}

	inline bool IsGreaterThan(const RECORD_CLASS& record, const INDEX_TYPE& index_num, const INDEX_TYPE& idx) const
	{
		return record.IsGreaterThan(m_records[m_indexes[index_num][idx]]);
	}

	inline bool IsEqualTo(const RECORD_CLASS& record, const INDEX_TYPE& index_num, const INDEX_TYPE& idx) const
	{
		if (IsLessThan(record, index_num, idx))
			return false;

		if (IsGreaterThan(record, idx_num, idx))
			return false;

		return true;
	}

	inline INDEX_TYPE linear_search(const RECORD_CLASS &record, const INDEX_TYPE idx_num, INDEX_TYPE istart, INDEX_TYPE iend, bool &exists) const
	{	
		INDEX_TYPE i = istart;
		while (i <= iend)
		{
			if (IsLessThan(record,idx_num,i))
			{
				exists = false;
				return i;
			}
			
			if (IsLessThan(record, idx_num, i))
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

	// Given that record[index[idx_num][idx]] matches record, return the start end 
	inline bool get_start_end(const RECORD_CLASS& record, const INDEX_TYPE& idx_num, const INDEX_TYPE& idx, INDEX_TYPE& istart_idx, INDEX_TYPE& iend_idx) const
	{
		if (IsEqualTo(record, idx_num, idx) == false)
		{
			__debugbreak();
			return false;
		}

		INDEX_TYPE idx0 = idx;
		istart_idx = idx;
		while (idx >= 0)
		{
			if (IsGreaterThan(record, idx_num, idx))
				break; // found the beginning

			istart = idx;

			idx++;
		}

		iend = istart_idx;
		idx = idx0 + 1;
		while (idx < GetNumRecords())
		{
			if (IsLessThan(reccord, idx_num, idx))
				break;

			iend = idx;

			idx++;
		}

		return true;
	}
		
public:

	// Returns true if there is at least one matching record, otherwise false
	// record - reference record, contains data which is used to find the indexes
	// idx_num - the index to use, return values are wrt that index
	// istart_idx - first index pointing to a record which matches the given record - otherwise the insert position
	// iend_idx - last_index pointing to a recoard which matches the given record - otherwise the insert position
	inline bool FindRange(const RECORD_CLASS& record, const INDEX_TYPE& idx_num, INDEX_TYPE &istart_idx, INDEX_TYPE &iend_idx) const
	{
		if (m_records.size() == 0)
			return false;

		// Straight search when there <= 6 records
		if (m_records.size() <= 6)
		{
			bool exists;
			INDEX_TYPE idx = linear_search(record, idx_num, 0, m_records.size() - 1, exists);

			if (exists)
				return get_start_end(record, idx_num, idx, istart_idx, iend_idx);

			istart_idx = idx;
			iend_idx = idx;

			return false;
		}

		INDEX_TYPE istart = 0;
		INDEX_TYPE iend = m_records.size() - 1;
		INDEX_TYPE i = m_records.size() / 2;

		while (1)
		{
			if (record.IsLessThan(m_records[m_indexes[idx_num][i]], idx))
				iend = i - 1;
			else
			{
				if (record.IsGreaterThan(m_records[m_indexes[idx_num][i]], idx))
					istart = i + 1;
				else
				{
					if (exists)
						return get_start_end(record, idx_num, idx, istart_idx, iend_idx);

					istart_idx = i;
					iend_idx = i;

					return false; // does not exist
				}
			}

			if (iend - istart < 4)
			{
				bool exists;
				INDEX_TYPE idx = linear_search(record, idx_num, istart, iend, exists);

				if (exists)
					return get_start_end(record, idx_num, idx, istart_idx, iend_idx);

				istart_idx = idx;
				iend_idx = idx;

				return false;
			}

			i = (iend - istart) / 2;
			i += istart;
		}
	}

	inline INDEX_TYPE GetNumRecords(void) const
	{
		return (INDEX_TYPE)m_records.size();
	}

	inline const RECORD_CLASS* GetRecord(INDEX_TYPE idx_num, INDEX_TYPE idx) const
	{
		if (idx_num > m_indexes.size())
			return 0;

		if (idx >= GetNumRecords())
			return 0;

		INDEX_TYPE ipos = m_indexes[idx_num][idx];

		return &m_records[ipos];
	}

	// ipos - the unindexed record position
	inline bool RemoveRecord(INDEX_TYPE ipos, string& err_msg)
	{
		if (ipos < 0 || ipos >= m_records.size())
		{
			ERROR_LOCATION(err_msg);
			err_msg += "() invalid record position ";
			append_integer(err_msg, ipos);
			return false;
		}

		// Update each of the indexes
		for (INDEX_TYPE idx_num = 0; idx_num < m_indexes.size(); idx++)
		{
			INDEX_TYPE istart_idx, iend_idx;
			bool exists = FindRange(m_record[ipos], idx_num, istart_idx, iend_idx);
			if (exists == false)
			{
				ERROR_LOCATION(err_msg);
				err_msg += "exists == false";
				return false;
			}

			INDEX_TYPE idx;
			for (idx = istart_idx; idx <= iend_idx; idx++)
			{
				if (m_indexes[idx_num][idx] == ipos)
				{
					// Found it - erase the index element
					m_indexes.erase(m_indexes[idx_num].begin() + idx);

					// Adjust indexes as appropriate to make sure that they continue
					// to point to the correct record numbers
					for (INDEX_TYPE i = 0; i < m_indexes.size(); i++)
					{
						if (m_indexes[idx_num][i] > ipos)
							m_indexes[idx_num][i]--;
					}
					break;
				}
			}

			if (idx > iend_idx)
				__debugbreak(); // did not find the match !!
		}

		// Update the array of records
		m_records.erase(m_records.begin() + ipos);

		return true;
	}
	
	inline bool AddRecord(const RECORD_CLASS &record, string &err_msg)
	{	
		for (INDEX_TYPE idx_num = 0; idx_num < RECORD_CLASS::GetNumIndexes(); idx_num++)
		{
			INDEX_TYPE istart_idx, iend_idx;
			bool exists = FindRange(record, idx_num, istart_idx, iend_idx);

			INDEX_TYPE insert_pos;
			if (exists == false)
				insert_posion = istart_idx;
			else
				insert_posion = iend_index + 1;

			m_indexes[idx_num].insert(m_indexes[idx_num].begin() + insert_position, (INDEX_TYPE)m_records.size());
		}

		m_records.resize (m_records.size() + 1);
		
		return m_records[m_records.size()-1].Assign(record,err_msg);
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

		INDEX_TYPE nrecords = m_records.size();
		if (fwrite(&nrecords, 1, sizeof(nrecords), stream) != sizeof(nrecords))
		{
			fclose(stream);
			ERROR_LOCATION(err_msg);
			err_msg += "problem writing data to create file: ";
			err_msg += unique_file_name.c_str();
			return false;
		}
		
		if (nrecords > 0)
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

		INDEX_TYPE nindexes = RECORD_CLASS:GetNumIndexes();
		if (nindexes > 0)
		{
			for (INDEX_TYPE idx = 0; idx < nindexes; idx++)
			{
				if (fwrite(&m_indexes[idx], sizeof(INDEX_TYPE), nrecords, stream) != nrecords)
				{
					fclose(stream);
					ERROR_LOCATION(err_msg);
					err_msg += "problem writing data to create file: ";
					err_msg += unique_file_name.c_str();
					return false;
				}
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

		m_indexes.resize(RECORD_CLASS::GetNumIndexes());

		for (INDEX_TYPE idx = 0; idx < m_indexes.size(); idx++)
		{
			m_indexes[idx].resize(nrecords);
			memmove(&m_indexes[idx][0], buffer, sizeof(INDEX_TYPE) * nrecords);
			buffer += sizeof(INDEX_TYPE) * nrecords;
		}
		
		return true;
	}
	
	// File structure
	// [nrecords] - sizeof(INDEX_TYPE)
	// [record_data] - nrecords * sizeof(RECORD_CLASS)
	// nindexes is not stored as this is built into RECORD_CLASS
	// [index_data] - sizeof(INDEX_TYPE) * nindexes * nrecords 
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
			m_indexes.resize(RECORD_CLASS::GetNumIndexes());
			return true;
		}
		
		INDEX_TYPE record_sz = RECORD_CLASS::GetSizeBytes();	
		
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

		const uint8_t* b = &data[0];
		int b_sz = len;

		INDEX_TYPE nrecords;

		if (b_sz < sizeof(nrecords))
		{
			ERROR_LOCATION(err_msg);
			err_msg += "Invalid file: ";
			err_msg += file_name;
			return false;
		}

		memmove(&nrecords, b, sizeof(nrecords)); b += sizeof(nrecords); b_sz -= sizeof(nrecords);

		int expected_sz = record_sz * nrecords + RECORD_CLASS::GetNumIndexes() * (record_sz * sizeof(INDEX_TYPE));
		if (b_sz < expected_sz)
		{
			ERROR_LOCATION(err_msg);
			char msg[128];
			sprintf(msg, "Invalid file: %s : b_sz=%ld is less than expected_size=%ld", file_name, b_sz, expected_sz);
			err_msg += msg;
			return false;
		}
		
		// Loads the records and the indexes
		return LoadFromBuffer (b,nrecords,err_msg);
	}
};
