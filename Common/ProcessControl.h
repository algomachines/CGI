#pragma once

// Start a process and return when it has finished
inline bool RunProcess
(
	const char* exe_file,
	const char* working_directory,
	const char* params,
	const char* target_file,
	string& err_msg,
	DWORD timeout_seconds/*=INFINITE*/,
	DWORD* exit_code/*=0*/,
	bool bShowProcess/*=false*/
)
{
	BOOL				bInheritHandles;
	DWORD				dwCreationFlags;
	STARTUPINFO			StartupInfo;

	PROCESS_INFORMATION ProcessInfo;

	// This parameter must be true in order to pipe the stdout and stderr streams
	bInheritHandles = TRUE;
	dwCreationFlags = 0;
	memset(&StartupInfo, 0, sizeof(StartupInfo));
	memset(&ProcessInfo, 0, sizeof(ProcessInfo));

	StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
	if (bShowProcess == false)
		StartupInfo.wShowWindow = SW_HIDE;
	else
		StartupInfo.wShowWindow = SW_SHOW;

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	DWORD dw_open_status = CREATE_ALWAYS;
	DWORD dw_desired_access = GENERIC_WRITE | GENERIC_READ;
	if (_access(target_file, 00) == 0)
	{
		dw_open_status = OPEN_EXISTING;
		dw_desired_access = FILE_APPEND_DATA;
	}

	HANDLE hfile = CreateFile(target_file, dw_desired_access, FILE_SHARE_WRITE | FILE_SHARE_READ, &sa, dw_open_status, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hfile == INVALID_HANDLE_VALUE)
	{
		DWORD dwLastError = GetLastError();

		err_msg = "RunProcess () Problem creating file: ";
		err_msg += target_file;
		err_msg += ", error: ";
		err_msg += dwLastError;
		return false;
	}

	StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
	StartupInfo.hStdOutput = hfile;
	StartupInfo.hStdError = INVALID_HANDLE_VALUE;
	StartupInfo.hStdInput = INVALID_HANDLE_VALUE;

	///////////////////////////////////////////////////////////////////////////////
	// Construct the command line 
	string s_command_line = exe_file;
	s_command_line += " ";
	s_command_line += params;

	int len = (int)strlen(s_command_line.c_str());
	char* command_line = new char[len + 1];
	strcpy_s(command_line, len + 1, (const char*)s_command_line.c_str());

	BOOL status = CreateProcess(NULL,
		command_line,
		NULL,
		NULL,
		bInheritHandles,
		dwCreationFlags,
		NULL,					// environment block - use Create's
		working_directory,
		&StartupInfo,
		&ProcessInfo);

	memset(command_line, 0, len + 1);
	delete[] command_line;
	command_line = 0;

	if (status == FALSE)
	{
		// CreateProcess failed - find out why and tell user
		DWORD dwLastError;
		dwLastError = GetLastError();

		err_msg = "RunProcess () starting process ";
		err_msg += exe_file;
		err_msg += ", error: ";
		err_msg += dwLastError;

		return false;
	}

	if (WaitForSingleObject(ProcessInfo.hProcess, timeout_seconds == INFINITE ? INFINITE : timeout_seconds * 1000) == WAIT_TIMEOUT)
	{
		err_msg = "RunProcess () timeout waiting for ";
		err_msg += command_line;

		if (exit_code)
			GetExitCodeProcess(ProcessInfo.hProcess, exit_code);

		TerminateProcess(ProcessInfo.hProcess, 0);

		CloseHandle(ProcessInfo.hProcess);
		CloseHandle(ProcessInfo.hThread);

		CloseHandle(hfile);
		return false;
	}
	else
	{
		if (exit_code)
			GetExitCodeProcess(ProcessInfo.hProcess, exit_code);
	}

	CloseHandle(ProcessInfo.hProcess);
	CloseHandle(ProcessInfo.hThread);

	CloseHandle(hfile);
	return true;
};
