#ifdef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sacagalib.h"

#define CONNECTING_STATE 0
#define READING_STATE    1

typedef struct {
	OVERLAPPED oOverlap;
	HANDLE hPipe;
	char request[WIN32_PIPE_BUFSIZE];
	DWORD howMuchToRead;
	DWORD state; // CONNECTING_STATE or READING_STATE
	int isIOPending;
} pipe_struct;

pipe_struct Pipe[WIN32_MAX_PIPES];
HANDLE hEvents[WIN32_MAX_PIPES];

void disconnect_and_reconnect(DWORD);
int connect_to_new_client(HANDLE, LPOVERLAPPED);
void write_to_log_file(pipe_struct*);

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

		// Call the subroutine to connect to the new client
		Pipe[i].isIOPending = connect_to_new_client(Pipe[i].hPipe, &Pipe[i].oOverlap);

		Pipe[i].state = Pipe[i].isIOPending ? CONNECTING_STATE : // still connecting
				READING_STATE;     // ready to read
	}

	while (1) {
		// Wait for the event object to be signaled, indicating
		// completion of an overlapped read, write, or
		// connect operation.
		whichPipe = WaitForMultipleObjects(
				WIN32_MAX_PIPES, // number of event objects
				hEvents,         // array of event objects
				FALSE,           // does not wait for all
				INFINITE         // waits indefinitely
		);
		// whichPipe shows which pipe completed the operation.
		i = whichPipe - WAIT_OBJECT_0;  // determines which pipe
		if (i < 0 || i > (WIN32_MAX_PIPES - 1)) {
			write_log(ERROR, "Index out of range.\n");
			return 0;
		}

		// Get the result if the operation was pending.
		if (Pipe[i].isIOPending) {
			fSuccess = GetOverlappedResult(
					Pipe[i].hPipe,     // handle to pipe
					&Pipe[i].oOverlap, // OVERLAPPED structure
					&howManyBytes,     // bytes transferred
					FALSE              // do not wait
			);
			switch (Pipe[i].state) {
			// Pending connect operation
			case CONNECTING_STATE:
				if (!fSuccess) {
					write_log(ERROR, "Error %ld.\n", GetLastError());
					return 0;
				}
				Pipe[i].state = READING_STATE;
				break;

			// Pending read operation
			case READING_STATE:
				if (!fSuccess || howManyBytes == 0) {
					disconnect_and_reconnect(i);
					continue;
				}
				Pipe[i].howMuchToRead = howManyBytes;
				break;

			default: {
				write_log(ERROR, "Invalid pipe state.\n");
				return 0;
			}
			}
		}

		// The pipe state determines which operation to do next.
		switch (Pipe[i].state) {
		// READING_STATE:
		// The pipe instance is connected to the client
		// and is ready to read a request from the client.
		case READING_STATE:
			fSuccess = ReadFile(
					Pipe[i].hPipe,
					Pipe[i].request,
					WIN32_PIPE_BUFSIZE * sizeof(TCHAR),
					&Pipe[i].howMuchToRead,
					&Pipe[i].oOverlap
			);
			// The read operation completed successfully.
			if (fSuccess && Pipe[i].howMuchToRead != 0) {
				Pipe[i].isIOPending = FALSE;
				continue;
			}
			// The read operation is still pending.
			if (!fSuccess && (GetLastError() == ERROR_IO_PENDING)) {
				Pipe[i].isIOPending = TRUE;
				continue;
			}

			write_to_log_file(&Pipe[i]);

			// An error occurred; disconnect from the client.
			disconnect_and_reconnect(i);
			break;

		default: {
			printf("Invalid pipe state.\n");
			return 0;
		}
		}
	}
	return 0;
}

// This function is called when an error occurs or when the client
// closes its handle to the pipe. Disconnect from this client, then
// call ConnectNamedPipe to wait for another client to connect.
void disconnect_and_reconnect(DWORD i) {
	// Disconnect the pipe instance.
	if (! DisconnectNamedPipe(Pipe[i].hPipe) ) {
		write_log(ERROR, "DisconnectNamedPipe failed with %ld.\n", GetLastError());
	}

	// Call a subroutine to connect to the new client.
	Pipe[i].isIOPending = connect_to_new_client(
		Pipe[i].hPipe,
		&Pipe[i].oOverlap);

	Pipe[i].state = Pipe[i].isIOPending ?
		CONNECTING_STATE : // still connecting
		READING_STATE;     // ready to read
}

// This function is called to start an overlapped connect operation.
// It returns TRUE if an operation is pending or FALSE if the
// connection has been completed.
int connect_to_new_client(HANDLE hPipe, LPOVERLAPPED lpo) {
	int fConnected, isIOPending = FALSE;

	// Start an overlapped connection for this pipe instance.
	fConnected = ConnectNamedPipe(hPipe, lpo);

	// Overlapped ConnectNamedPipe should return zero.
	if (fConnected) {
		write_log(ERROR, "ConnectNamedPipe failed with %ld.\n", GetLastError());
		return 0;
	}

	switch (GetLastError()) {
	// The overlapped connection in progress.
	case ERROR_IO_PENDING:
		isIOPending = TRUE;
		break;

	// Client is already connected, so signal an event.
	case ERROR_PIPE_CONNECTED:
		if (SetEvent(lpo->hEvent))
		break;

	// If an error occurs during the connect operation...
	default: {
		write_log(ERROR, "ConnectNamedPipe failed with %ld.\n", GetLastError());
		return 0;
	}
	}

	return isIOPending;
}

void write_to_log_file(pipe_struct* pipe) {
	DWORD bytesWritten;
	// Sleep(500);
	HANDLE hLogFile = CreateFileA(
			SACAGAWEALOGS_PATH,
			FILE_APPEND_DATA,
			FILE_SHARE_READ, // Security arrtibutes: 0 means the file is locked
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
			hLogFile,                // open file handle
			pipe->request,         // start of data to write
			strlen(pipe->request), // number of bytes to write
			&bytesWritten,         // number of bytes that were written
			NULL                     // no overlapped structure
	);
	if (!bErrorFlag) {
		write_log(WARNING, "WriteFile %s failed with error: %d",
				SACAGAWEALOGS_PATH, GetLastError());
	}
	CloseHandle(hLogFile);

	// clean the pipe's buffer
	ZeroMemory(pipe->request, WIN32_PIPE_BUFSIZE);
}

#endif
