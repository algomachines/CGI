#include "OS.h"

#ifdef _DEBUG
#define ENABLE_DEBUGGING 
#else
//#define ENABLE_DEBUGGING 
#endif

#include "memory_tools.h"
#include "file_tools.h"
#include "random_number.h"

#include "WindowsTypes.h"
#include "SimpleDB.hpp"
//#include "IndexedDB.hpp"

#include "time_tools.h"
#include "Encryption.h"

#include "console_tools.hpp"

#include "DRM_ProgramRecord.h"
#include "DRM_PrivateMessageRecord.h"
#include "ProcessControl.h"

const char* ownership_reg_db_file_name = "../DRM/DB.bin";			 // Generated - Program ID database
const char* messages_db_file_name = "../DRM/MSG.bin";				 // Generated - Message database

const char* generated_code_dir = "../DRM/Generated";							// Created at install time time with correct security / priviledges
const char* backup_dir = "../DRM/Backup";
const char* compiler_exe = "../DRM/Generated/Compiler.exe";						// Installed
const char* code_template_file = "../DRM/Generated/modify_guid_template.code";	// Installed

const char* CGI_name = "PrivateMessenger";

// MSG.bin holds unsent messages. Record size for this db is 32+32+8+256=328 bytes
// 3000 unsent messages -> MSG.bin = 984,000 bytes
#define MAX_PENDING_MESSAGES 3000

// Stale pending messages will be deleted when we reach the MAX_UNSENT_MESSAGES limit
// 7*24*3600*1000 = 604,800,000 - one week
#define STALE_MESSAGE_TIME_LIMIT_MS 604800000LL

// A particular sender may not exceed this number of pending messages
// If a client sends a message which will exceed this limit, then the oldest
// pending message from that client is deleted to make room for the new pending message.
#define MAX_PENDING_MESSAGES_PER_SENDER 20

// record size is 32+8+8+8+8+16 = 80 bytes = sizeof(DRM_ProgramRecord)
// 800,000 size limit on this file restricts the number of clients to 10,000
// DB.bin
#define OWNERSHIP_DB_MAX_SIZE 800000

const int MAX_CLIENTS = OWNERSHIP_DB_MAX_SIZE / sizeof(DRM_ProgramRecord);

bool BackupOwnershipDB(void);

// Customize this per the install
const uint8_t AdminID[32] = { 0xaf, 0xfe, 0x8f, 0x25, 0x3b, 0x3c, 0xbf, 0x20,
							  0x8d, 0xd0, 0x63, 0xec, 0x21, 0xa6, 0x2a, 0xa2,
							  0xbe, 0xd, 0x69, 0x37, 0x17, 0x37, 0x16, 0xa9,
							  0x96, 0xea, 0x52, 0xeb, 0xae, 0xda, 0xbf, 0x96 };

/////////////////////////////////////////////////////////////////////////
// START COM PROTOCOL ///////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Send Message 
// 
//[My ID] 32 bytes - this will be my program ID if I have one, otherwise a random 32 byte buffer
//[Hash of ID I want to communicate with] 32 bytes - this will be the hash of a program ID or of an outside ID
//[Message size] 2 bytes 
//[Message] number of bytes as specified by message size, ascii text only

///////////////////////////////////////////////////////////////////////
// Receive Next Message
//
//[My ID] 32 bytes
//
// Return value:
//[Hash of ID of sender] - 32 bytes
//[Content size] - 2 bytes - zero if there are no messages
//[Content] - size bytes

///////////////////////////////////////////////////////////////////////
// Clean Old Messages
//
//[Master ID] 32 bytes - must match file
//[Time_ms] 8 bytes - clean messages which were originated before this time 

// END COM PROTOCOL /////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////
// RULES ////////////////////////////////////////////////////////////////
// 1) Limit total number of unread messages stored at one time - DONE
// 2) Delete messages as soon as they are read - DONE
// 3) Mechanism to delete unread messages after some period - configurable - DONE
// 4) Messages are ascii text only - DONE
// 5) Only one unread message from a partilar sender to a particular recipient at a time - DONE
// 6) Only a limited number of unread messages from a particular sender at a time - DONE
// 7) Messages are limited to 256 characters each - DONE
// 8) *Only a limited number of unread messages for a particular receiver at a time - configurable

// * - still need to be done

//////////////////////////////////////////////////////////////////////////
// DATA STORAGE
// 1) An indexed database containining unread messages
//	a) Sender ID - 32 bytes
//	b) Receiver Hashed ID - 32 bytes
//  c) Timestamp - 8 bytes
//  d) Message - 256 bytes
// 
// Total of 328 bytes per message
//
// Sort order for the db is b)
// Indexes are maintained for a) and c)

inline bool CheckPendingMessageLimits(SimpleDB<DRM_PrivateMessageRecord>& db, const uint8_t* hashed_sender_id)
{
	if (db.GetNumRecords() >= MAX_PENDING_MESSAGES) // We are at the maximum number of unsent messages
	{
		// remove stale pending messages
		uint32_t idx = 0;
		uint64_t tnow = get_time_ms();
		while (idx < db.GetNumRecords())
		{
			const DRM_PrivateMessageRecord* rec = db.GetRecordByIndex(idx);

			if (!rec)
				break; // should not happen

			uint64_t t = rec->GetTimestamp();

			if (tnow < t)
				break; // should not happen

			if (tnow - t < STALE_MESSAGE_TIME_LIMIT_MS)
			{
				idx++;
				continue;
			}

			// Remove the stale message
			string err_msg;
			if (db.RemoveRecord(idx, err_msg) == false)
				break; // should not happen
		}
	}

	if (db.GetNumRecords() >= MAX_PENDING_MESSAGES)
		return false; // can't add any more pending messages

	// Check pending message count for this client.
	// If we are about to exceed the pending message limit, then delete the oldest pending message before adding this one.
	{
		uint32_t n = 0;
		uint32_t idx = 0;

		uint64_t t_oldest = get_time_ms();
		uint32_t idx_oldest = 0xFFFFFFFF;

		while (idx < db.GetNumRecords())
		{
			const DRM_PrivateMessageRecord* rec = db.GetRecordByIndex(idx);

			if (!rec)
				break; // should not happen

			if (memcmp(rec->GetHashedIDSender(), hashed_sender_id, 32) == 0)
			{
				if (t_oldest > rec->GetTimestamp())
				{
					t_oldest = rec->GetTimestamp();
					idx_oldest = idx;
				}

				n++;
			}

			idx++;
		}

		if (n > MAX_PENDING_MESSAGES_PER_SENDER)
		{
			string err_msg;
			if (db.RemoveRecord(idx_oldest, err_msg) == false)
				return false;
		}
	}

	return true;
}

// [OP == 1] 1 byte
//[Hashed recipient ID] 32 bytes - this will be the hash of a program ID of the recipient
//[Message size] 2 bytes 
//[Message] number of bytes as specified by message size, ascii text only
inline bool SendPrivateMessage(const DRM_ProgramRecord *prog_rec, const uint8_t* buf, int buf_sz)
{
	if (ID_SIZE_BYTES != 32)
	{
		DEBUG_ERROR("Invalid ID size");
		CacheStdout("0001");
		return false;
	}

	if (buf_sz < ID_SIZE_BYTES + 2 + 1)
	{

		DEBUG_ERROR("Invalid buf_sz, must be at least 67");
		CacheStdout("0002");
		return false;
	}

	SimpleDB<DRM_PrivateMessageRecord> db;
	string err_msg;

	if (DoesFileExist(messages_db_file_name))
	{
		if (db.LoadFromFile(messages_db_file_name, err_msg) == false)
		{
			DEBUG_ERROR(err_msg.c_str());
			CacheStdout("0003");
			return false;
		}
	}

	const uint8_t* hashed_id_sender = prog_rec->GetID();
	const uint8_t* hashed_id_receiver = buf;

	buf += 32;

	// Limit the total database size
	if (CheckPendingMessageLimits(db, hashed_id_sender) == false)
	{
		DEBUG_ERROR("Exceeded sender limit.");
		CacheStdout("0004");
		return false;
	}

	const uint8_t* buf0 = buf;

#ifdef ENABLE_DEBUGGING
	string s_sender, s_receiver;
	bin_to_ascii_char(hashed_id_sender, 32, s_sender);
	bin_to_ascii_char(hashed_id_receiver, 32, s_receiver);
#endif

	DRM_PrivateMessageRecord rec;

	rec.SetHashedIDSender(hashed_id_sender); 
	rec.SetHashedIDReceiver(hashed_id_receiver); 

	uint16_t msg_len;
	memmove(&msg_len, buf, 2); buf += 2;

	rec.SetMessage(buf, msg_len); buf += msg_len;

	uint64_t time_ms = get_time_ms();
	rec.SetTimestamp(&time_ms);

	bool changes_made;
	if (db.UpdateRecord(rec, changes_made, err_msg) == false)
	{
		DEBUG_ERROR(err_msg.c_str());
		CacheStdout("0005");
		return false;
	}

	if (changes_made)
	{
		if (db.GetNumRecords() > MAX_PENDING_MESSAGES)
		{
			DEBUG_ERROR("Can't add more unsent messages, limit has been reached");
			CacheStdout("0006");
			return false;
		}

		if (db.SaveToFile(messages_db_file_name, err_msg) == false)
		{
			DEBUG_ERROR(err_msg.c_str());
			CacheStdout("0007");
			return false;
		}

#ifdef ENABLE_DEBUGGING
		string report_file = messages_db_file_name;
		report_file += ".txt";
		if (db.GenerateReport(report_file.c_str(), err_msg) == false)
		{
			DEBUG_ERROR(err_msg.c_str());
		}
#endif

	}

	CacheStdout("0000");
	return true;
}

// key is a 16 byte random buffer created on the server
// seed is a 4 byte random buffer created on the server
// ev is an 8 byte random buffer created on the server
inline void create_random_values(const uint8_t* hashed_id, uint8_t* key, uint32_t& seed, uint64_t& ev)
{
	vector<uint8_t> rbuf;
	rbuf.resize(0x1000);
	create_random_buffer(&rbuf[0], rbuf.size(), (uint32_t)100); // 100 ms processing time

	// Create a random 16 byte key value
	memmove(key, &rbuf[sizeof(seed)], 16);

	// Only the seed value and the hashed_id will be used to create the expected value
	uint32_t e0, e1;
	MurmurHash3_x86_32(hashed_id, ID_SIZE_BYTES, seed, &e0);
	MurmurHash3_x86_32(hashed_id, ID_SIZE_BYTES, e0, &e1);

	ev = e1;
	ev = ev << 32;
	ev = ev + e0;
}

// hashed_id - 32 byte value from the client
// key - 32 byte value from the server
// seed - 4 byte value from the server
// ev - 8 byte vlaue from the server
bool generate_source_code(const char *code_template_file, const char *generated_code_directory, const uint8_t* hashed_id, const uint8_t* key, uint32_t& seed, uint64_t& ev, string& source_code_file, string& err_msg)
{
	if (DoesFileExist(code_template_file) == FALSE)
	{
		err_msg = "Missing file: ";
		err_msg += (const char*)code_template_file;
		return false;
	}

	if (DoesFileExist(generated_code_directory) == false)
	{
		if (_mkdir(generated_code_directory))
		{
			err_msg = "Unable to create directory: ";
			err_msg += (const char*)generated_code_directory;
			return false;
		}
	}

	int flen = filelength(code_template_file);

	FILE* stream = 0;
	fopen_s(&stream, code_template_file, "rb");

	vector<char> code;
	code.resize(flen);

	if (fread(&code[0], 1, flen, stream) != flen)
	{
		fclose(stream);
		err_msg = "Problem reading from file: ";
		err_msg += code_template_file;
	}

	fclose(stream);

	char s_seed[32];
	sprintf_s(s_seed, sizeof(s_seed), "0x%08lX", seed);

	int n = replace_string(code, "#SEED#", s_seed);

	if (n == 0)
	{
		err_msg = "Did not find \"#SEED#\" in file: ";
		err_msg += (const char*)code_template_file;
		return false;
	}

	if (n > 1)
	{
		err_msg = "Found more than one instance of \"#SEED#\" in file: ";
		err_msg += (const char*)code_template_file;
		return false;
	}

	char s_ev[32];
	sprintf_s(s_ev, "0x%016llX", ev);

	n = replace_string(code, "#EV#", s_ev);

	if (n == 0)
	{
		err_msg = "Did not find \"#EV#\" in file: ";
		err_msg += (const char*)code_template_file;
		return false;
	}

	//if (n > 1)
	//{
	//    err_msg = "Found more than one instance of \"#EV#\" in file: ";
	//    err_msg += (const char*)code_template_file;
	//    return false;
	//}

	string s_key;
	bin_to_hex_char(key, 16, s_key);

	n = replace_string(code, "#KEY#", s_key.c_str());

	if (n == 0)
	{
		err_msg = "Did not find \"#KEY#\" in file: ";
		err_msg += (const char*)code_template_file;
		return false;
	}

	if (n > 1)
	{
		err_msg = "Found more than one instance of \"#KEY#\" in file: ";
		err_msg += (const char*)code_template_file;
		return false;
	}

	string fname;
	bin_to_ascii_char(hashed_id, 32, fname);
	source_code_file = generated_code_directory;
	source_code_file += "/";
	source_code_file += fname.c_str();
	source_code_file += ".code";

	fopen_s(&stream, source_code_file.c_str(), "wb");

	if (!stream)
	{
		err_msg = "Unable to create file: ";
		err_msg += source_code_file.c_str();
		return false;
	}

	if (fwrite(&code[0], 1, code.size(), stream) != code.size())
	{
		fclose(stream);
		err_msg = "Problem writing to file: ";
		err_msg += source_code_file.c_str();
		return false;
	}

	fclose(stream);

	return true;
}

bool RunCompiler(const char* source_code_file, const char* pwd, const char* compiled_code_file, string &err_msg)
{
	if (DoesFileExist(compiler_exe) == false)
	{
		err_msg = "File does not exist: ";
		err_msg += compiler_exe;
		return false;
	}

	// Compiler <source_file> [compiled_code_file] [pwd] [params_file]

	char working_directory[1024];
	_getcwd(working_directory, sizeof(working_directory));
	//strcat_s(working_directory, sizeof(working_directory), "\\..\\DRM");

	//const char* working_directory = "C:\\Program Files\\Apache Software Foundation\\Apache2.4\\DRM";

	char params[1024];
	sprintf_s(params, sizeof(params), "\"%s\" \"%s\" \"%s\"", source_code_file, compiled_code_file, pwd);

	string target_file = source_code_file;
	target_file += ".txt";

	DWORD timeout_sec = 20;
	DWORD exit_code = 0;
	bool bShowProcess = false;

	uint64_t t0 = get_time_ms();

	bool status = RunProcess
	(
		compiler_exe,
		working_directory,
		params,
		target_file.c_str(),
		err_msg,
		timeout_sec,
		&exit_code,
		bShowProcess
	);

	uint64_t t1 = get_time_ms();

#ifdef ENABLE_DEBUGGING
	// log to the target file
	FILE* stream = 0;
	fopen_s(&stream, target_file.c_str(), "a");
	if (stream)
	{
		fprintf_s(stream, "\ncompiler_exe: %s\n", compiler_exe);
		fprintf_s(stream, "working_directory: %s\n", working_directory);
		fprintf_s(stream, "params: %s\n", params);
		fprintf_s(stream, "err_msg: %s\n", err_msg.c_str());
		fprintf_s(stream, "exit_code: %ld\n", exit_code);
		fprintf_s(stream, "status: %s\n", status ? "true" : "false");
		fprintf_s(stream, "elapsed_ms: %lld\n", t1 - t0);

		fclose(stream);
	}
#else
	_unlink(target_file.c_str());
#endif

	if (status == false)
		return false;

	if (exit_code != 0)
	{
		err_msg = "Compiler fails, exit code: ";
		char tmp[100];
		sprintf_s(tmp, sizeof(tmp), "%04lX", exit_code);
		err_msg += tmp;
		return false;
	}

	return true;
}

// OP == 0
//
//[My hashed ID] - 32 bytes
//
// Return value:
//[size of compiled binary] - 2 bytes
//[body of compiled binary] - N bytes
//
// Note: Password for the compiled binary is My hashed ID
// On error the size of the compiled binary return value is 0
//
inline const DRM_ProgramRecord* AddClient(const uint8_t* buf, int buf_sz, SimpleDB<DRM_ProgramRecord> &db)
{
	if (db.GetNumRecords() >= MAX_CLIENTS)
	{
		DEBUG_ERROR("DB.bin has the maximum number of clients already.");
		CacheStdout("0000");
		return 0;
	}

	if (ID_SIZE_BYTES != 32)
	{
		DEBUG_ERROR("Invalid ID size");
		CacheStdout("0000");
		return 0;
	}

	if (buf_sz != ID_SIZE_BYTES)
	{

		DEBUG_ERROR("Invalid buf_sz, must 32");
		CacheStdout("0000");
		return 0;
	}

	string err_msg;

	const uint8_t* hashed_id = buf;
	
	DRM_ProgramRecord rec;
	rec.SetID(hashed_id);

	if (db.GetRecord(rec))
	{
		DEBUG_ERROR("ID already exists.");
		CacheStdout("0000");
		return 0;
	}

	// These random values are used to generate source code which will be compiled and sent to the client by the server
	uint8_t key[16];
	uint32_t seed;
	uint64_t ev;
	create_random_values(hashed_id, key, seed, ev);

	//const char* generated_code_directory = "Generated";
	string source_code_file;
	if (generate_source_code(code_template_file, generated_code_dir, hashed_id, key, seed, ev, source_code_file, err_msg) == false)
	{
		DEBUG_ERROR(err_msg.c_str());
		CacheStdout("0000");
		return 0;
	}

	// compiled_code_file has extension .bin
	string compiled_code_file = source_code_file;
	compiled_code_file.erase(compiled_code_file.length() - 4, 4);
	compiled_code_file += "bin";

	string s_hashed_id;
	bin_to_ascii_char(hashed_id, ID_SIZE_BYTES, s_hashed_id);

	// Compile the source to a bytecode file
	uint64_t t0 = get_time_ms();
	bool status = RunCompiler(source_code_file.c_str(), s_hashed_id.c_str(), compiled_code_file.c_str(), err_msg);
	uint64_t t1 = get_time_ms();
	uint64_t delta = t1 - t0;

	char msg[1024];
	sprintf_s(msg, sizeof(msg), "Compile time: %llu ms\n", delta);

	DEBUG_MSG(msg);

	if (status == false)
	{
		DEBUG_ERROR2("Compiler error", err_msg.c_str());
		CacheStdout("0000");
		return 0;
	}

	// read the bytecode file
	vector<uint8_t> bin;

	int sz = filelength(compiled_code_file.c_str());
	bin.resize(sz);

	FILE* stream = fopen(compiled_code_file.c_str(), "rb");
	if (!stream)
	{
		DEBUG_ERROR2("Problem opening file for reading: ", compiled_code_file);
		CacheStdout("0000");
		return 0;
	}

	if (fread(&bin[0], 1, bin.size(), stream) != bin.size())
	{
		fclose(stream);
		DEBUG_ERROR2("Problem reading file: ", compiled_code_file);
		CacheStdout("0000");
		return 0;
	}

	fclose(stream);

	uint16_t sz_u16 = bin.size();
	CacheBinStdout(&sz_u16, sizeof(sz_u16)); // cache the sz of the message
	CacheBinStdout(&bin[0], bin.size());

	bool changes_made = false;
	if (db.UpdateRecord(rec, changes_made, err_msg) == false)
	{
		DEBUG_ERROR(err_msg.c_str());
		return 0;
	}

	const DRM_ProgramRecord *prog_rec = db.GetRecord(rec);

	if (prog_rec)
		prog_rec->SetKey(key); // make sure that the new record has the key which has just be geneated

#ifndef ENABLE_DEBUGGING
	_unlink(compiled_code_file.c_str());
	_unlink(source_code_file.c_str());
#endif
	
	return prog_rec;
}

// Receive Pending Messages - from any senders
// [OP == 2] 1 byte 
//
// If the hashed ID is not in the instance db then return 00 - i.e. no messages pending
// If the hashed ID is in the instance db then return pending messages if there are any
// Hashed IDs are added to the instance db via a separate operation
// 
// Return value:
//[Status message] - 4 bytes, "0000" for success
//[size of message1] - 1 byte
//[Hash of ID of sender1] - 32 bytes
//[Timestamp of msg] - 8 bytes
//[Message1] 
//[size of message2] - 1 byte
//[Hash of ID of sender2] - 32 bytes
//[Timestamp of msg] - 8 bytes
//[message2] 
// ... 
//[size of message] - 1 byte - value is zero, indicates that there are no more messages
//
// do_not_return_messges - under some circumstances we do not want to return any messgages - e.g. during a recovery operation
inline bool ReceivePendingMessages(const DRM_ProgramRecord *prog_rec, bool do_not_return_messages)
{
	string err_msg;

	SimpleDB<DRM_PrivateMessageRecord> db;

	if (DoesFileExist(messages_db_file_name))
	{
		if (db.LoadFromFile(messages_db_file_name, err_msg) == false)
		{
			DEBUG_ERROR(err_msg.c_str());
			CacheStdout("0001");
			return false;
		}
	}

	DRM_PrivateMessageRecord token;

	token.SetHashedIDReceiver(prog_rec->GetID());  // SenderID will be zeros

#ifdef ENABLE_DEBUGGING
	string s_receiver_id;
	bin_to_ascii_char(prog_rec->GetID(), 32, s_receiver_id);
#endif

	bool exists;
	uint32_t idx = db.GetRecordIndex(token, exists);

	if (exists)
	{
		DEBUG_ERROR("Internal error.");
		CacheStdout("0003");
		return false;
	}

	if (idx == db.GetNumRecords() || do_not_return_messages)
	{
		DEBUG_MSG("No messages");
		CacheStdout("0000"); // Success 
		CacheStdout("00"); // No messages
		return true;
	}

	bool changes_made = false;

	// Need to insert the status string later
	int insert_pos = CacheGetInsertPosition();

	string s;

#ifdef ENABLE_DEBUGGING
	FILE* stream = 0;
#endif

	while (idx < db.GetNumRecords())
	{
		const DRM_PrivateMessageRecord* rec = db.GetRecordByIndex(idx);

#ifdef ENABLE_DEBUGGING
		string s_rec_receiver_id, s_rec_sender_id;
		bin_to_ascii_char((const uint8_t *)rec->GetHashedIDReciever(), 32, s_rec_receiver_id);
		bin_to_ascii_char((const uint8_t*)rec->GetHashedIDSender(), 32, s_rec_sender_id);
#endif

		if (memcmp(rec->GetHashedIDReciever(), prog_rec->GetID(), ID_SIZE_BYTES))
			break;

		// unread message found
		uint8_t hashed_id_sender[ID_SIZE_BYTES];
		memmove(hashed_id_sender, rec->GetHashedIDSender(), ID_SIZE_BYTES);

		string s_msg;
		rec->GetMessage(s_msg);
		int msg_len = strlen(s_msg.c_str());

		if (db.RemoveRecord(idx, err_msg) == false)
		{
			DEBUG_ERROR(err_msg.c_str());
			CacheStdout("0004", 0,0, insert_pos);
			return false;
		}

		changes_made = true;

		if (msg_len == 0)
			continue;

		uint8_t sz = (uint8_t)min(msg_len, 256);
		CacheStdout(bin_to_hex_char(&sz, 1, s)); // cache the sz of the message

		string s_hashed_id_sender;
		bin_to_hex_char(hashed_id_sender, ID_SIZE_BYTES, s_hashed_id_sender);
		CacheStdout(s_hashed_id_sender.c_str()); // cache the hashed id of the sender

		uint64_t t = rec->GetTimestamp();
		string s_timestamp;
		bin_to_hex_char((const uint8_t*)&t, sizeof(t), s_timestamp);
		CacheStdout(s_timestamp.c_str());

		CacheStdout(s_msg.c_str());

#ifdef ENABLE_DEBUGGING
		if (stream == 0)
		{
			string log_file;
			log_file = messages_db_file_name;
			log_file += ".sent.txt";
			fopen_s(&stream, log_file.c_str(), "w");
		}

		if (stream)
		{
			fprintf_s(stream, "[%lu]\n", idx);
			fprintf_s(stream, "hashed_ID_sender: %s\n", s_rec_sender_id.c_str());
			fprintf_s(stream, "hashed_ID_receiver: %s\n", s_rec_receiver_id.c_str());
			fprintf_s(stream, "Timestamp_ms: %llu\n", t);
			fprintf_s(stream, "%s\n", s_msg.c_str());
		}
#endif
	}

#ifdef ENABLE_DEBUGGING
	if (stream)
		fclose(stream);
#endif

	// Send out the termination character
	uint8_t term_byte = 0;
	CacheStdout(bin_to_hex_char(&term_byte, 1, s));

	if (changes_made)
	{
		if (db.SaveToFile(messages_db_file_name, err_msg) == false)
		{
			DEBUG_ERROR(err_msg.c_str());
		}
	}

	CacheStdout("0000", 0,0, insert_pos); // insert success indicator
	return true;
}

// Clean all pending messages older than the specified time
//
//[Time_ms] 8 bytes - clean messages which were originated before this time
//
inline bool CleanOldMessages(const DRM_ProgramRecord *prog_rec, const uint8_t *buf, int buf_sz)
{
	if (memcmp(prog_rec->GetID(), AdminID, ID_SIZE_BYTES))
	{
		DEBUG_ERROR("Given ID does not match AdminID.");
		CacheStdout("Fail");
		return false;
	}

	uint64_t t_ms;
	memmove(&t_ms, buf, sizeof(t_ms));

	uint64_t t_now = get_time_ms();

	if (t_ms > t_now)
	{
		DEBUG_ERROR("t_ms > t_now");
		CacheStdout("Fail");
		return false;
	}


	SimpleDB<DRM_PrivateMessageRecord> db;
	string err_msg;

	if (DoesFileExist(messages_db_file_name) == false)
	{
		char msg[1024];
		sprintf(msg, "Success - message db does not exist: %s", messages_db_file_name);
		DEBUG_MSG(msg);
		CacheStdout(msg);
		return true;
	}


	if (db.LoadFromFile(messages_db_file_name, err_msg) == false)
	{
		DEBUG_ERROR(err_msg.c_str());
		CacheStdout("Fail");
		return false;
	}

	int n = 0;
	for (uint32_t idx = 0; idx < db.GetNumRecords(); idx++)
	{
		const DRM_PrivateMessageRecord* rec = db.GetRecordByIndex(idx);
		if (rec->GetTimestamp() < t_ms)
		{
			if (db.RemoveRecord(idx, err_msg) == false)
			{
				DEBUG_ERROR(err_msg.c_str());
				CacheStdout("Fail");
				return false;
			}

			n++;
		}
	}

	if (n)
	{
		if (db.SaveToFile(messages_db_file_name, err_msg) == false)
		{
			DEBUG_ERROR(err_msg.c_str());
			CacheStdout("Fail");
			return false;
		}
	}

	char msg[1024];
	sprintf(msg, "Success - %ld messages removed", n);
	DEBUG_ERROR(msg);
	CacheStdout(msg);
	return true;
}


// key is 16 bytes
// 
// buf[]
// hashed_id of client - 32 bytes
// instance_hash - 8 bytes - decrypted with modified guid 
// remainder - ?? bytes - to be decrypted with modified guid
const DRM_ProgramRecord *decrypt_with_modified_guid(uint8_t* buf, int buf_sz, const GUID& leading_guid, SimpleDB<DRM_ProgramRecord> &db)
{
	if (buf_sz <= ID_SIZE_BYTES + 8)
	{
		DEBUG_ERROR2(__FUNCTION__,"() Invalid buf_sz");
		return 0;
	}

	string err_msg;

	const uint8_t* hashed_id = buf;

	DRM_ProgramRecord token;
	token.SetID(hashed_id);

	const DRM_ProgramRecord *prog_rec = db.GetRecord(token);

#ifdef ENABLE_DEBUGGING
	string primary_check;
	bin_to_ascii_char(prog_rec->GetID(), 32, primary_check);
#endif

	if (prog_rec == 0)
	{
		DEBUG_ERROR("ID does not exist.");
		return 0;
	}

	GUID modified_leading_guid;
	create_modified_guid(prog_rec->GetKey(), (const uint8_t*)&leading_guid, (uint8_t*)&modified_leading_guid);

	symmetric_encryption(buf + ID_SIZE_BYTES, buf_sz - ID_SIZE_BYTES, modified_leading_guid);

	return prog_rec;
}

bool open_program_record_database(SimpleDB<DRM_ProgramRecord>& db)
{
	string err_msg;
	if (DoesFileExist(ownership_reg_db_file_name))
	{
		// This should not happen, the DB.bin only grows with the AddNewClient() procedure and that procedure won't allow this
		if (filelength(ownership_reg_db_file_name) > OWNERSHIP_DB_MAX_SIZE)
		{
			char msg[1024];
			sprintf_s(msg, sizeof(msg), "file %s is too large, must be less than %ld bytes", ownership_reg_db_file_name, OWNERSHIP_DB_MAX_SIZE);
			DEBUG_ERROR(msg);
			return false;
		}

		if (db.LoadFromFile(ownership_reg_db_file_name, err_msg) == false)
		{
			DEBUG_ERROR(err_msg.c_str());
			return false;
		}
	}

	return true;
}

bool validate_instance_hash(const uint8_t* b, int buf_sz, const DRM_ProgramRecord* prog_rec, bool& matches_prev)
{
	matches_prev = false;

	uint8_t instance_hash[16];
	get_instance_hash(prog_rec, instance_hash);

	bool success = memcmp(b, &instance_hash, sizeof(instance_hash)) == 0;

	if (success == false)
	{
		if (get_instance_hash_prev(prog_rec, instance_hash))
			matches_prev = memcmp(b, &instance_hash, sizeof(instance_hash)) == 0;
	}

#ifdef ENABLE_DEBUGGING
	string s;
	bin_to_ascii_char(b, 16, s);
	DEBUG_MSG2("instance_hash[in] ", s.c_str());

	if (success == false)
		DEBUG_MSG("instance hash does not match");
#endif

	return success;
}

inline void modify_item(const char*& item, vector<char>& static_str, const char* s_find, const char* s_replace)
{
	int len = strlen(item);
	static_str.resize(len + 1);
	memmove(&static_str[0], item, len);
	replace_string(static_str, s_find, s_replace);
	item = &static_str[0];
}

static string s_fname;

static vector<char> s_ownership_reg_db_file_name;		// = "../DRM/DB.bin";								// Generated - Program ID database
static vector<char> s_messages_db_file_name;			// = "../DRM/MSG.bin";								// Generated - Message database

static vector<char> s_generated_code_dir;				// = "../DRM/Generated";							// Created at install time with correct security / priviledges
static vector<char> s_backup_dir;						// = "../DRM/Backup";								// Created at install time wiith correct security / priviledges
static vector<char> s_compiler_exe;						// = "../DRM/Generated/Compiler.exe";				// Installed
static vector<char> s_code_template_file;				// = "../DRM/Generated/modify_guid_template.code";	// Installed

inline void construct_names_and_paths(const char* executable)
{
	const char* c0 = 0;
#ifdef WIN32
	c0 = strrchr(executable, '\\');
#endif

	if (c0 == 0)
		c0 = strrchr(executable, '/');
	
	if (c0 == 0)
		s_fname = "PrivateMessenger";
	else
	{
		c0++;
		const char *c1 = strrchr(c0, '.');

		if (c1)
		{
			int len = (int)(c1 - c0);
			s_fname.assign(c0, len);
		}
		else
		{
			s_fname = c0;
		}
	}

	CGI_name = s_fname.c_str();

	const char* s_find = "/DRM/";

	string s_replace = "/";
	s_replace += CGI_name;
	s_replace += "/";

	modify_item(ownership_reg_db_file_name, s_ownership_reg_db_file_name, s_find, s_replace.c_str());
	modify_item(messages_db_file_name, s_messages_db_file_name, s_find, s_replace.c_str());
	modify_item(generated_code_dir, s_generated_code_dir, s_find, s_replace.c_str());
	modify_item(backup_dir, s_backup_dir, s_find, s_replace.c_str());
	modify_item(compiler_exe, s_compiler_exe, s_find, s_replace.c_str());
	modify_item(code_template_file, s_code_template_file, s_find, s_replace.c_str());
}

int main(int argc, const char** argv)
{
	construct_names_and_paths(argv[0]);

	const char* s_content_length = getenv("CONTENT_LENGTH");

	vector<char> s_content_len_storage;
	const char* content = 0;
	if (s_content_length == 0)
	{
		if (argc < 2)
			return 0;

		// Try getting content from argv[1] - command line parameter
		content = argv[1];
		int len = strlen(content);

		s_content_len_storage.resize(10);

		sprintf(&s_content_len_storage[0], "%ld", len);

		s_content_length = &s_content_len_storage[0];
	}

	printf("Content-type: text/html\n\n");

	DEBUG_MSG2("Component: ", CGI_name);

	bool valid_content = true;  int content_length = 0;
	if (sscanf(s_content_length, "%d", &content_length) != 1 || content_length > 500)
	{
		if (g_debug) fprintf(g_debug_stream, "invalid : content_length: %s, expected no more than 500\n", s_content_length);
		return 0;
	}

	string s;
	s.resize(content_length);

	if (content == 0)
	{
		for (int i = 0; i < content_length; i++)
			s[i] = (char)getchar();
	}
	else
	{
		s = content;
	}

	DEBUG_MSG2("input: ", s.c_str());

	string err_msg;

	vector<uint8_t> buf;
	if (hex_char_to_bin(s, buf) == false)
	{
		DEBUG_ERROR("hex_char_to_bin() failure");
		return 0;
	}

	GUID leading_guid;
	memmove(&leading_guid, &buf[0], sizeof(GUID));

	buf.erase(buf.begin(), buf.begin() + sizeof(GUID));

	// Only the op byte and the client hashed id is encrypted with the leading guid
	symmetric_encryption(&buf[0], 1+ID_SIZE_BYTES, leading_guid);

	int op = buf[0];
	uint8_t* hashed_id = &buf[1];

	// need global semaphore lock here
	char semaphore_name[256];
	sprintf_s(semaphore_name, "Global_%s", CGI_name);
	HANDLE h_semaphore = CreateSemaphore(NULL, 1, 1, semaphore_name);

	if (h_semaphore == NULL)
	{
		char msg[1024];
		sprintf_s(msg, sizeof(msg), "Failed trying to createsemaphore: %s : code: %lu", semaphore_name, GetLastError());
		DEBUG_ERROR(msg);
		return 0;
	}

	// Wait up to 10 seconds
	DWORD scode = WaitForSingleObject(h_semaphore, 10000);
	if (scode != WAIT_OBJECT_0)
	{
		char msg[1024];
		sprintf_s(msg, sizeof(msg), "Failed waiting for semaphore: %s : code: %lu", semaphore_name, scode);
		DEBUG_ERROR(msg);
		return 0;
	}

	SimpleDB<DRM_ProgramRecord> prog_db;
	const DRM_ProgramRecord* prog_rec = 0;

	uint8_t* b = &buf[1];
	int buf_sz = buf.size() - 1;

	////////////////////////////////////////////////////////////////////////////////////////
	// For NewClient command, op == 0
	//[leading_guid] - 16 bytes
	//[op] - one byte, encrypted with leading guid 
	//[hashed id of client] - 32 bytes, encrypted with leading guid

	///////////////////////////////////////////////////////////////////////////////////////
	// For anything other than AddClient command
	//[leading_guid] - 16 bytes
	//[op] - one byte, encrypted with leading guid 
	//[hashed id of client] - 32 bytes, encrypted with leading guid
	//[instance_hash] - 16 bytes, encrypted with modified_leading_guid
	//[data] - ?? bytes - varies by the op, encrypted with modified_leading_guid

	// NOTE: Instance_hash is only computed on the server. The client stores
	// this value. This value changes every time a client successfully queries the server.
	// Possession of an active instance_hash signifies ownership of the 
	// client ID, access to its messages, being able to send messages as that id, etc.
	//
	// Instance_hash is necessairly a 1:1 mapping between a particular client program and
	// a particular server.

	// Q. Why do we send bytecode to the client?
	// 
	// A. This provides added security to client-server communications.
	// 
	// 1) The algorithm which is used to encrypt/decrypt client-server communications:
	// 	a) Depends on the byte code implented algo to scramble the guid
	//	a) Is not included in client source code - so the client source may be disclosed without destroying security
	//	b) Can't be run except by someone who has access to the client ID.
	//     Note that the client ID is created by the client, stored with password encryption on the client.

	while (1)
	{
		if (open_program_record_database(prog_db) == false)
		{
			CacheStdout("0103");
			break;
		}

		// Special processing for the AddClient operation
		if (op == 0)
		{
			// Add a new client ID to the client id instance database
			// If successful, the compiled binary code for encrypting messages is returned
			prog_rec = AddClient(b, buf_sz, prog_db);
			break;
		}

		//////////////////////////////////////////////////////////////
		// Standard processing

		int data_sz = buf_sz - ID_SIZE_BYTES;

		if (data_sz < 8) // Data must consist of at least an instance hash, which is 8 bytes
		{
			CacheStdout("0100");
			DEBUG_ERROR("Invalid data sz");
			break;
		}

		// Validate the hashed client ID and decrypt the data following the hashed client ID
		prog_rec = decrypt_with_modified_guid(b, buf_sz, leading_guid, prog_db);
		if (prog_rec == 0) 
			break;

		// Advance past the hashed client ID
		b += ID_SIZE_BYTES;
		buf_sz -= ID_SIZE_BYTES;

		// next up is the hashed client instance
		// This will fail if the server and the client don't agree on the instance
		bool matches_prev = false;
		if (validate_instance_hash(b, buf_sz, prog_rec, matches_prev) == false)
		{
			if (op != 2 && matches_prev) // If the instance_hash is wrong, but matches the previous instance_hash
			{							 // and if the command is ReceivePendingMessages, then proceed. 
				CacheStdout("0101");	 // We will not return any messages, but we will send the correct instance hash.
				break;
			}
		}

		// Advance past the hashed client instance
		b += 16;
		buf_sz -= 16;

		if (op == 1)  { SendPrivateMessage(prog_rec, b, buf_sz); break; }
		if (op == 2)  { ReceivePendingMessages(prog_rec, matches_prev); break; }
		//if (op == 99) { CleanOldMessages(prog_rec, b, buf_sz); break; }

		CacheStdout("0102");
		DEBUG_ERROR("undefined op");

		break;
	}

	bool modify_leading_guid = (op != 0);
	bool increment_nqueries = (op != 2); // not increment nqueries / instance_hash for ReceivePendingMessages 
	if (op != 2) increment_nqueries = true;

	if (increment_nqueries)
	{
		prog_rec->IncrementNQueries();

		BackupOwnershipDB();

		// Save the modified prog_rec immediately.
		// Failure here will not affect the client's synchronization with the server
		if (prog_db.SaveToFile(ownership_reg_db_file_name, err_msg) == false)
		{
			LONG prev;
			ReleaseSemaphore(h_semaphore, 1, &prev);

			return 0; // don't send anything to the client if we fail at this point
		}
	}

	// Send data to the client, including the upated instance_hash. Any failure here:
	// 
	// 1) client does not receive the buffer
	// 2) client receives corrupted buffer
	// 3) client crashes before saving the new instance hash
	//
	// will be recovered through the recovery process.
	SendEncryptedCachedStdout(prog_rec, modify_leading_guid);

	LONG prev;
	ReleaseSemaphore(h_semaphore, 1, &prev);

	return 0;
}

BOOL replace_backup(const char* source_file, const char* target_file, int min)
{
	if (DoesFileExist(source_file) == false)
		return FALSE; // no backup

	uint64_t t_now = get_time_ms();

	uint64_t t_file;
	if (get_file_time_ms(target_file, t_file) == false)
	{
		return CopyFile(source_file, target_file, FALSE);
	}
	else
	{
		uint64_t delta_ms = min;
		delta_ms *= 60;
		delta_ms *= 1000;

		if ((t_file < t_now) && ((t_now - t_file) > delta_ms)) 
		{
			return CopyFile(source_file, target_file, FALSE);
		}
	}

	return FALSE;
}

bool BackupOwnershipDB(void)
{
	if (DoesFileExist(backup_dir) == false)
		return false;

	if (DoesFileExist(ownership_reg_db_file_name) == false)
		return false;

	char file_name[1024];

	const int t_min[5] = { 5, 20, 60, 240, 1440 }; // 5min, 20min, 1h, 4h, 1d

	vector<string> file_names;
	file_names.resize(5);

	// Create the backup file names
	for (int i = 0; i < file_names.size(); i++)
	{
		sprintf_s(file_name, "%s/DB.[%04ld].bin", backup_dir, t_min[i]);
		file_names[i] = file_name;
	}

	for (int i = file_names.size() - 1; i > 0; i--)
		replace_backup(file_names[i - 1].c_str(), file_names[i].c_str(), t_min[i]);

	replace_backup(ownership_reg_db_file_name, file_names[0].c_str(), t_min[0]);

	return true;
}