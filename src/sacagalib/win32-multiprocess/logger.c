#ifdef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sacagalib.h"

#define CONNECTING_STATE 0
#define READING_STATE    1
#define WRITING_STATE    2

typedef struct {
	OVERLAPPED oOverlap;
	HANDLE hPipeInst;
	char chRequest[WIN32_PIPE_BUFSIZE];
	DWORD cbRead;
	char chReply[WIN32_PIPE_BUFSIZE];
	DWORD cbToWrite;
	DWORD dwState;
	int fPendingIO;
} PIPEINST, *LPPIPEINST;

VOID DisconnectAndReconnect(DWORD);
BOOL ConnectToNewClient(HANDLE, LPOVERLAPPED);
VOID GetAnswerToRequest(LPPIPEINST);

PIPEINST Pipe[WIN32_MAX_PIPES];
HANDLE hEvents[WIN32_MAX_PIPES];

int main(int argc, char *argv[]) {
	DWORD i, dwWait, cbRet, dwErr;

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
		Pipe[i].hPipeInst = CreateNamedPipeA(
				WIN32_PIPE_NAME,         // pipe name
				PIPE_ACCESS_DUPLEX |     // read/write access
				FILE_FLAG_OVERLAPPED,    // overlapped mode
				PIPE_TYPE_MESSAGE |      // message-type pipe
				PIPE_READMODE_MESSAGE |  // message-read mode
				PIPE_WAIT,               // blocking mode
				WIN32_MAX_PIPES,         // number of instances
				WIN32_PIPE_BUFSIZE * sizeof(char),  // output buffer size
				WIN32_PIPE_BUFSIZE * sizeof(char),  // input buffer size
				INFINITE,                // client time-out
				NULL);                   // default security attributes

		if (Pipe[i].hPipeInst == INVALID_HANDLE_VALUE) {
			write_log(ERROR, "CreateNamedPipe failed with error: %I64d",
					GetLastError());
			return 0;
		}

		// Call the subroutine to connect to the new client
		Pipe[i].fPendingIO = ConnectToNewClient(Pipe[i].hPipeInst, &Pipe[i].oOverlap);

		Pipe[i].dwState = Pipe[i].fPendingIO ? CONNECTING_STATE : // still connecting
				READING_STATE;     // ready to read
	}

	while (1) {
		// Wait for the event object to be signaled, indicating
		// completion of an overlapped read, write, or
		// connect operation.
		dwWait = WaitForMultipleObjects(
				WIN32_MAX_PIPES,    // number of event objects
				hEvents,      // array of event objects
				FALSE,        // does not wait for all
				INFINITE      // waits indefinitely
		);
		// dwWait shows which pipe completed the operation.
		i = dwWait - WAIT_OBJECT_0;  // determines which pipe
		if (i < 0 || i > (WIN32_MAX_PIPES - 1)) {
			write_log(ERROR, "Index out of range.\n");
			return 0;
		}

		// Get the result if the operation was pending.
		if (Pipe[i].fPendingIO) {
			fSuccess = GetOverlappedResult(
					Pipe[i].hPipeInst, // handle to pipe
					&Pipe[i].oOverlap, // OVERLAPPED structure
					&cbRet,            // bytes transferred
					FALSE              // do not wait
			);
			switch (Pipe[i].dwState) {
			// Pending connect operation
			case CONNECTING_STATE:
				if (! fSuccess) {
					printf("Error %ld.\n", GetLastError());
					return 0;
				}
				Pipe[i].dwState = READING_STATE;
				break;

			// Pending read operation
			case READING_STATE:
				if (!fSuccess || cbRet == 0) {
					DisconnectAndReconnect(i);
					continue;
				}
				Pipe[i].cbRead = cbRet;
				Pipe[i].dwState = WRITING_STATE;
				break;

			// Pending write operation
			case WRITING_STATE:
				if (!fSuccess || cbRet != Pipe[i].cbToWrite) {
					DisconnectAndReconnect(i);
					continue;
				}
				Pipe[i].dwState = READING_STATE;
				break;

			default: {
				write_log(ERROR, "Invalid pipe state.\n");
				return 0;
			}
			}
		}

		// The pipe state determines which operation to do next.
		switch (Pipe[i].dwState) {
		// READING_STATE:
		// The pipe instance is connected to the client
		// and is ready to read a request from the client.
		case READING_STATE:
			fSuccess = ReadFile(
					Pipe[i].hPipeInst,
					Pipe[i].chRequest,
					WIN32_PIPE_BUFSIZE * sizeof(TCHAR),
					&Pipe[i].cbRead,
					&Pipe[i].oOverlap
			);
			// The read operation completed successfully.
			if (fSuccess && Pipe[i].cbRead != 0) {
				Pipe[i].fPendingIO = FALSE;
				Pipe[i].dwState = WRITING_STATE;
				continue;
			}
			// The read operation is still pending.
			dwErr = GetLastError();
			if (!fSuccess && (dwErr == ERROR_IO_PENDING)) {
				Pipe[i].fPendingIO = TRUE;
				continue;
			}

			// An error occurred; disconnect from the client.
			DisconnectAndReconnect(i);
			break;

		// WRITING_STATE:
		// The request was successfully read from the client.
		// Get the reply data and write it to the client.
		case WRITING_STATE:
			GetAnswerToRequest(&Pipe[i]);
			fSuccess = WriteFile(
					Pipe[i].hPipeInst,
					Pipe[i].chReply,
					Pipe[i].cbToWrite,
					&cbRet,
					&Pipe[i].oOverlap
			);
			// The write operation completed successfully.
			if (fSuccess && cbRet == Pipe[i].cbToWrite) {
				Pipe[i].fPendingIO = FALSE;
				Pipe[i].dwState = READING_STATE;
				continue;
			}

			// The write operation is still pending.
			dwErr = GetLastError();
			if (!fSuccess && (dwErr == ERROR_IO_PENDING)) {
				Pipe[i].fPendingIO = TRUE;
				continue;
			}

			// An error occurred; disconnect from the client.
			DisconnectAndReconnect(i);
			break;

		default: {
			printf("Invalid pipe state.\n");
			return 0;
		}
		}
	}

	return 0;

}


// DisconnectAndReconnect(DWORD)
// This function is called when an error occurs or when the client
// closes its handle to the pipe. Disconnect from this client, then
// call ConnectNamedPipe to wait for another client to connect.

VOID DisconnectAndReconnect(DWORD i) {
	// Disconnect the pipe instance.
	if (! DisconnectNamedPipe(Pipe[i].hPipeInst) ) {
		write_log(ERROR, "DisconnectNamedPipe failed with %ld.\n", GetLastError());
	}

	// Call a subroutine to connect to the new client.
	Pipe[i].fPendingIO = ConnectToNewClient(
		Pipe[i].hPipeInst,
		&Pipe[i].oOverlap);

	Pipe[i].dwState = Pipe[i].fPendingIO ?
		CONNECTING_STATE : // still connecting
		READING_STATE;     // ready to read
	}

	// ConnectToNewClient(HANDLE, LPOVERLAPPED)
	// This function is called to start an overlapped connect operation.
	// It returns TRUE if an operation is pending or FALSE if the
	// connection has been completed.
BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo) {
	BOOL fConnected, fPendingIO = FALSE;

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
		fPendingIO = TRUE;
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

	return fPendingIO;
}

VOID GetAnswerToRequest(LPPIPEINST pipe) {
	DWORD dwBytesWritten;
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
	BOOL bErrorFlag = WriteFile(
			hLogFile,              // open file handle
			pipe->chRequest,    // start of data to write
			strlen(pipe->chRequest) + 1,     // number of bytes to write
			&dwBytesWritten,    // number of bytes that were written
			NULL              // no overlapped structure
	);
	if (!bErrorFlag) {
		write_log(WARNING, "WriteFile %s failed with error: %d",
				SACAGAWEALOGS_PATH, GetLastError());
	}
	CloseHandle(hLogFile);
	// write_log(DEBUG, "[%p] %s\n", pipe->hPipeInst, pipe->chRequest);
	// strncpy(pipe->chReply, "Default answer from server", WIN32_PIPE_BUFSIZE);
	// pipe->cbToWrite = (lstrlen(pipe->chReply) + 1) * sizeof(TCHAR);
}

#endif
