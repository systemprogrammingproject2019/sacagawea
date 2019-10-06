#ifdef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sacagalib.h"

typedef struct {
	OVERLAPPED oOverlap;
	HANDLE hPipe;
} pipe_struct;

char request[WIN32_PIPE_BUFSIZE]; // that will contain the LOGstring read from pipe

pipe_struct Pipe[WIN32_MAX_PIPES];
HANDLE hEvents[WIN32_MAX_PIPES];

void disconnect_and_reconnect(DWORD);
void connect_to_new_client(HANDLE, LPOVERLAPPED);
void write_to_log_file(char*);

int WINAPI loggerConsoleEventHandler(DWORD fdwCtrlType) {
	// "return false" kills the process
	switch (fdwCtrlType)
	{
	case CTRL_BREAK_EVENT:
		return TRUE; // dont close the process
	default:
		write_log(DEBUG, "Quitting sacagawea-logger.exe!");
		return FALSE;
	}
	return FALSE;
}

int main(int argc, char *argv[]) {
	DWORD i, whichPipe, howManyBytes;

	if (!SetConsoleCtrlHandler(loggerConsoleEventHandler, true)) {
		write_log(ERROR, "SetConsoleCtrlHandler Failed with error: %lld", GetLastError());
		exit(1);
	}

	int fSuccess;
	// The initial loop creates several instances of a named pipe
	// along with an event object for each instance.  An
	// overlapped ConnectNamedPipe operation is started for each instance
	for (i = 0; i < WIN32_MAX_PIPES; i++) {
		// Create an event object for this instance
		hEvents[i] = CreateEvent(
				NULL,    // default security attribute
				TRUE,    // manual-reset event
				TRUE,    // initial state = signaled
				NULL     // unnamed event object
		);
		if (hEvents[i] == NULL) {
			write_log(ERROR, "CreateEvent failed with error code %I64d",
					GetLastError());
			return 0;
		}

		Pipe[i].oOverlap.hEvent = hEvents[i];
		Pipe[i].hPipe = CreateNamedPipeA(
				WIN32_PIPE_NAME,         // pipe name
				PIPE_ACCESS_DUPLEX       // read/write access
				| FILE_FLAG_OVERLAPPED,  // overlapped mode
				PIPE_TYPE_MESSAGE        // message-type pipe
				| PIPE_READMODE_MESSAGE  // message-read mode
				| PIPE_WAIT,             // blocking mode
				WIN32_MAX_PIPES,         // number of instances
				WIN32_PIPE_BUFSIZE * sizeof(char),  // output buffer size
				WIN32_PIPE_BUFSIZE * sizeof(char),  // input buffer size
				INFINITE,                // client time-out
				NULL                     // default security attributes
		);
		if (Pipe[i].hPipe == INVALID_HANDLE_VALUE) {
			write_log(ERROR, "CreateNamedPipe failed with error: %I64d",
					GetLastError());
			return 0;
		}

		connect_to_new_client(Pipe[i].hPipe, &Pipe[i].oOverlap);
	}

	while (1) {
		// Wait for the event object to be signaled, indicating
		// completion of an overlapped read, or connect operation.
		whichPipe = WaitForMultipleObjects(
				WIN32_MAX_PIPES, // number of event objects
				hEvents,         // array of event objects
				FALSE,           // does not wait for all
				INFINITE         // waits indefinitely
		);
		// whichPipe shows which pipe completed the operation.
		i = whichPipe - WAIT_OBJECT_0;  // determines which pipe
		if (i < 0 || i > (WIN32_MAX_PIPES - 1)) {
			write_log(ERROR, "Pipe index out of range.");
			return 0;
		}

		fSuccess = ReadFile(
					Pipe[i].hPipe,
					request,
					WIN32_PIPE_BUFSIZE * sizeof(TCHAR),
					&howManyBytes,
					NULL
		);
		if (!fSuccess) {
			write_log(ERROR, "ReadFile from Pipe failed with %ld.\n", GetLastError());
			continue; 
		} 

		write_to_log_file(request);
		disconnect_and_reconnect(i);
	}
	return 0;
}

// This function is called when an error occurs or when the client
// closes its handle to the pipe. Disconnect from this client, then
// call ConnectNamedPipe to wait for another client to connect.
void disconnect_and_reconnect(DWORD i) {
	// Disconnect the pipe instance.
	if (!DisconnectNamedPipe(Pipe[i].hPipe)) {
		write_log(ERROR, "DisconnectNamedPipe failed with %ld.\n", GetLastError());
	}
	// Call a subroutine to connect to the new client.
	connect_to_new_client(Pipe[i].hPipe, &Pipe[i].oOverlap);
}

// This function is called to start an overlapped connect operation.
// It returns TRUE if an operation is pending or FALSE if the
// connection has been completed.
void connect_to_new_client(HANDLE hPipe, LPOVERLAPPED lpo) {
	int fConnected;
	// Start an overlapped connection for this pipe instance.
	/* ConnectNamedPipe waits for a connection from a client
	   (in this case connectNamedPipe); it doesnt block because
	   we give it an LPOVERLAPPED != null and hPipe was created
	   with flag FILE_FLAG_OVERLAPPED. */
	fConnected = ConnectNamedPipe(hPipe, lpo);

	// Overlapped ConnectNamedPipe should return zero.
	if (fConnected) {
		write_log(ERROR, "ConnectNamedPipe failed with %ld.\n", GetLastError());
		return;
	}

	// manage ConnectedNamedPipe's return value
	switch (GetLastError()) {
		// in case of ERROR_IO_PENDING: it's just like EWOULDBLOCK, i.e. the call
		// wanted to block because it still has a pending incoming connection
		case ERROR_IO_PENDING:
			break;
		// in case of ERROR_PIPE_CONNECTED: between the start of the call
		// CreateNamedPipe and the call ConnectNamedPipe, someone connected to the pipe.
		case ERROR_PIPE_CONNECTED:
			if (SetEvent(lpo->hEvent)) {
				break;
			}
		// If an error occurs during the connect operation...
		default: {
			write_log(ERROR, "ConnectNamedPipe failed with %ld.\n", GetLastError());
		}
	}

}

void write_to_log_file(char* pipe) {
	DWORD bytesWritten;

	HANDLE hLogFile = CreateFileA(
			SACAGAWEALOGS_PATH,
			FILE_APPEND_DATA,
			FILE_SHARE_READ,
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL
	);
	if (hLogFile == INVALID_HANDLE_VALUE) {
		write_log(WARNING, "CreateFileA %s failed with error: %d",
				SACAGAWEALOGS_PATH, GetLastError());
		CloseHandle(hLogFile);
		return;
	}

	int bErrorFlag = WriteFile(
			hLogFile,        // open file handle
			request,         // start of data to write
			strlen(request), // number of bytes to write
			&bytesWritten,   // number of bytes that were written
			NULL             // no overlapped structure
	);
	if (!bErrorFlag) {
		write_log(WARNING, "WriteFile %s failed with error: %d",
				SACAGAWEALOGS_PATH, GetLastError());
	}
	CloseHandle(hLogFile);

	// clean the pipe's buffer
	ZeroMemory(request, WIN32_PIPE_BUFSIZE);
}

#endif