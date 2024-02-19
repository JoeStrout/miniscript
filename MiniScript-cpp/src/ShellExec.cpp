//
//  ShellExec.cpp
//  MiniScript
//
//  Created by Joe Strout on 2/9/24.
//  Copyright © 2024 Joe Strout. All rights reserved.
//

#include "ShellExec.h"

#if _WIN32 || _WIN64
	#define WINDOWS 1
	#include <windows.h>
	#include <Shlwapi.h>
	#include <Fileapi.h>
	#include <direct.h>
	#include <locale>
	#include <codecvt>
#else
	#include <unistd.h>	// for read()
	#include <sys/wait.h>   // for waitpid()
#endif


namespace MiniScript {

#if WINDOWS

// Helper function to read from file descriptor into string
String readFromFd(HANDLE fd, bool trimTrailingNewline=true) {
	const int bufferSize = 1024;
	DWORD bytesRead = 0;
	CHAR buffer[bufferSize];
	bool bSuccess;
	String output;
	bool trimmed = false;
	
	for (;;) {
		bSuccess = ReadFile(fd, buffer, bufferSize-1, &bytesRead, NULL);
		if (!bSuccess || bytesRead == 0) break;
		buffer[bytesRead] = '\0';
		if (trimTrailingNewline and bytesRead < bufferSize-1 and bytesRead > 0 and buffer[bytesRead-1] == '\n') {
			// Efficiently trim \n or \r\n from the end of the buffer
			buffer[bytesRead-1] = '\0';
			if (bytesRead > 1 and buffer[bytesRead-2] == '\r') {
				buffer[bytesRead-2] = '\0';
			}
			trimmed = true;
		}

		String s(buffer, bytesRead+1);
		output += s;
	}

	if (trimTrailingNewline && !trimmed) {
		// Not-so-efficiently trim our final string, in the case where our data happened
		// to exactly align with the buffer size, so we couldn't know we were at the
		// end of it to trim it above.  (This is a rare edge case.)
		int cut = 0;
		if (output.LengthB() > 1 and output[-1] == '\n') {
			cut = 1;
			if (output.LengthB() > 2 and output[-2] == '\r') cut = 2;
		}
		if (cut) output = output.SubstringB(0, output.LengthB() - cut);
	}
	
	return output;
}

bool BeginExec(String cmd, double timeout, double currentTime, ValueList* outResult) {
	// This is the initial entry into `exec`.  Fork a subprocess to execute the
	// given command, and return a partial result we can use to check on its progress.

	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	HANDLE hChildStd_OUT_Rd = NULL;
	HANDLE hChildStd_OUT_Wr = NULL;
	HANDLE hChildStd_ERR_Rd = NULL;
	HANDLE hChildStd_ERR_Wr = NULL;

	// Create a pipe for the child process's STDOUT and STDERR.
	// Disable the INHERIT flag to ensure each handle is not inherited
	if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) return false;
	SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);
	if (!CreatePipe(&hChildStd_ERR_Rd, &hChildStd_ERR_Wr, &saAttr, 0)) return false;
	SetHandleInformation(hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFO siStartInfo;
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = hChildStd_ERR_Wr;
	siStartInfo.hStdOutput = hChildStd_OUT_Wr;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	PROCESS_INFORMATION piProcInfo;
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	// Start the child process.
	if (!CreateProcessA(NULL,
		(LPSTR)cmd.c_str(), // command line
		NULL,               // process security attributes
		NULL,               // primary thread security attributes
		TRUE,               // handles are inherited
		0,                  // creation flags
		NULL,               // use parent's environment
		NULL,               // use parent's current directory
		&siStartInfo,       // STARTUPINFO pointer
		&piProcInfo))       // receives PROCESS_INFORMATION
	{
		return false;
	}

	// Close handles to the stdin and stdout pipes no longer needed by the child process.
	// If they are not explicitly closed, there is no way to recognize that the child process has completed.
	CloseHandle(hChildStd_OUT_Wr);
	CloseHandle(hChildStd_ERR_Wr);

	// As our partial result, return a list with the pid, the two read pipes, and the final time.
	// (We're going to cast our HANDLE to an int, which is a little dicey but maybe we get away with it.
	ValueList data;
	data.Add(Value((int)piProcInfo.hProcess));
	data.Add(Value((int)piProcInfo.hThread));
	data.Add(Value((int)hChildStd_OUT_Rd));
	data.Add(Value((int)hChildStd_ERR_Rd));
	data.Add(Value(currentTime + timeout));
	*outResult = data;
	return true;
}

bool FinishExec(ValueList data, double currentTime, String* outStdout, String* outStderr, int* outStatus) {
	// Start by getting the pid, the two read pipes, and the final time out of the partial result.
	HANDLE hProcess = (HANDLE)data[0].IntValue();
	HANDLE hThread = (HANDLE)data[1].IntValue();
	HANDLE stdOutPipe = (HANDLE)data[2].IntValue();
	HANDLE stdErrPipe = (HANDLE)data[3].IntValue();
	double finalTime = data[4].DoubleValue();

	int returnCode;
	String stdoutContent, stderrContent;

	// Wait a short time for the child process to exit
	DWORD waitResult = WaitForSingleObject(hProcess, 0.01);
	if (waitResult == WAIT_TIMEOUT) {
		// Child process not finished yet.
		if (currentTime < finalTime) {
			// Not timed out, either — keep waiting.
			return false;
		}

		// We've waited too long.  Time out.
		stderrContent = "Timed out";
		returnCode = 124 << 8;	// (124 is status code used by `timeout` command)
	} else {
		// Child process completed successfully.  Huzzah!
		// Read output from pipes.
		stdoutContent = readFromFd(stdOutPipe);
		stderrContent = readFromFd(stdErrPipe);
		// Get the exit code.
		DWORD returnDword;
		if (!GetExitCodeProcess(hProcess, &returnDword)) {
			returnDword = (DWORD)-1; // Use -1 or another value to indicate that getting the exit code failed
		}
		returnCode = (int)returnDword;
	}

	// Close handles to the child process and its primary thread.
	CloseHandle(hProcess);
	CloseHandle(hThread);

	// Close the remaining pipe handles.
	CloseHandle(stdOutPipe);
	CloseHandle(stdErrPipe);

	// Return results.
	*outStdout = stdoutContent;
	*outStderr = stderrContent;
	*outStatus = returnCode;
	return true;
}


#else

// Helper function to read from file descriptor into string
String readFromFd(int fd, bool trimTrailingNewline=true) {
	String output;
	const int bufferSize = 1024;
	char buffer[bufferSize];
	ssize_t bytesRead;

	bool trimmed = false;
	while ((bytesRead = read(fd, buffer, bufferSize)) > 0) {
		if (trimTrailingNewline and bytesRead < bufferSize and bytesRead > 0 and buffer[bytesRead-1] == '\n') {
			// Efficiently trim \n or \r\n from the end of the buffer
			bytesRead--;
			if (bytesRead > 0 and buffer[bytesRead-1] == '\r') bytesRead--;
			trimmed = true;
		}
		output += String(buffer, bytesRead);
	}

	if (trimTrailingNewline && !trimmed) {
		// Not-so-efficiently trim our final string, in the case where our data happened
		// to exactly align with the buffer size, so we couldn't know we were at the
		// end of it to trim it above.  (This is a rare edge case.)
		int cut = 0;
		if (output.LengthB() > 1 and output[-1] == '\n') {
			cut = 1;
			if (output.LengthB() > 2 and output[-2] == '\r') cut = 2;
		}
		if (cut) output = output.SubstringB(0, output.LengthB() - cut);
	}
	
	return output;
}

bool BeginExec(String cmd, double timeout, double currentTime, ValueList* outResult) {
	// This is the initial entry into `exec`.  Fork a subprocess to execute the
	// given command, and return a partial result we can use to check on its progress.

	// Create a pipe each for stdout and stderr.
	// File descriptor 0 of each is the read end; element 1 is the write end.
	int stdoutPipe[2];
	int stderrPipe[2];
	pipe(stdoutPipe);
	pipe(stderrPipe);
	
	pid_t pid = fork(); // Fork the process
	
	if (pid == -1) {
		return false;	// Error("Failed to fork the child process.");
	} else if (pid == 0) {
		// Child process.
		
		// Redirect stdout and stderr to our pipes, and then close the read ends.
		dup2(stdoutPipe[1], STDOUT_FILENO);
		dup2(stderrPipe[1], STDERR_FILENO);
		close(stdoutPipe[0]);
		close(stderrPipe[0]);
		
		// Call the host environment's command processor.  Or if the command
		// is empty, then return a nonzero value iff the command processor exists.
		const char* cmdPtr = cmd.empty() ? NULL : cmd.c_str();
		int cmdResult = std::system(cmdPtr);
		cmdResult = WEXITSTATUS(cmdResult);
		
		// All done!  Exit the child process and return the result.
		exit(cmdResult);
	}
	// Parent process.
	
	// Close the write end of the pipes.
	close(stdoutPipe[1]);
	close(stderrPipe[1]);
	
	// As our partial result, return a list with the pid, the two read pipes, and the final time.
	ValueList data;
	data.Add(Value(pid));
	data.Add(Value(stdoutPipe[0]));
	data.Add(Value(stderrPipe[0]));
	data.Add(Value(currentTime + timeout));
	*outResult = data;
	return true;
}

bool FinishExec(ValueList data, double currentTime, String* outStdout, String* outStderr, int* outStatus) {
	// Start by getting the pid, the two read pipes, and the final time out of the partial result.
	int pid = data[0].IntValue();
	int stdoutPipe = data[1].IntValue();
	int stderrPipe = data[2].IntValue();
	double finalTime = data[3].DoubleValue();
	
	// Then, see if the child process has finished.
	int returnCode;
	String stdoutContent, stderrContent;
	int waitResult = waitpid(pid, &returnCode, WUNTRACED | WNOHANG);
	//std::cout << "waitpid returned " << waitResult << ", returnCode is " << returnCode << std::endl;
	if (waitResult <= 0) {
		// Child process not finished yet.
		if (currentTime < finalTime) {
			// Not timed out, either — keep waiting.
			return false;
		}

		// We've waited too long.  Time out.
		stderrContent = "Timed out";
		returnCode = 124 << 8;	// (124 is status code used by `timeout` command)
	} else {
		// Child process completed successfully.  Huzzah!
		// Read output from pipes.
		stdoutContent = readFromFd(stdoutPipe);
		stderrContent = readFromFd(stderrPipe);
	}
	// Close our pipes.
	close(stdoutPipe);
	close(stderrPipe);

	// Return results.
	*outStdout = stdoutContent;
	*outStderr = stderrContent;
	*outStatus = WEXITSTATUS(returnCode);
	return true;
}

#endif

}  // end of namespace MiniScript
