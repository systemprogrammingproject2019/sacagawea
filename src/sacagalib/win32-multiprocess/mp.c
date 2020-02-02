#ifdef _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sacagalib.h"

BOOL WINAPI mpConsoleEventHandler(DWORD fdwCtrlType) {
	// "return false" kills the process
	switch (fdwCtrlType)
	{
	case CTRL_BREAK_EVENT:
		return TRUE; // dont close the process
	default:
		return FALSE;
	}
	return FALSE;
}

int main(int argc, char *argv[]) {
	HANDLE hMapFile = (HANDLE) strtoll(argv[1], NULL, 10);
	if( hMapFile == 0 ){
		write_log(ERROR, "strtoll of mp.c Failed with error");
		exit(1);
	}
	if (!SetConsoleCtrlHandler(mpConsoleEventHandler, true)) {
		write_log(ERROR, "SetConsoleCtrlHandler Failed with error: %lld", GetLastError());
		exit(1);
	}

	// open file mapping
	LPTSTR pBuf = (LPTSTR) MapViewOfFile(
			hMapFile, // handle to map object
			FILE_MAP_ALL_ACCESS,  // read/write permission
			0,
			0,
			sizeof(client_args)
	);
	if (pBuf == NULL) {
		write_log(ERROR, "MapViewOfFile failed wirh error: %d",	GetLastError());
		exit(1);
	}

	// copying client_args into a local variable, so we can delete
	// the file mapping asap. This way, we dont risk it will never be closed
	// if the management_function fails
	client_args c;
	memcpy(&c, pBuf, sizeof(client_args));
	if( UnmapViewOfFile(pBuf) == 0 ){
		write_log(ERROR, "UnmapViewOfFile failed with error: %I64d",GetLastError());
	}
	if( CloseHandle(hMapFile) == 0 ){
		write_log(ERROR, "Close hMapFile failed wirh error: %d",GetLastError());
	}
	// if (DuplicateHandle(c->socket, c->socket) != STATUS_SUCCESS) {
	// 	write_log(ERROR, "DuplicateHandle failed with error: %d", GetLastError());
	// 	return 1;
	// }

	management_function(&c);
	exit(1);
}
#endif
