#ifdef _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sacagalib.h"

int main(int argc, char *argv[]) {
	WSADATA wsaData;
	if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) == SOCKET_ERROR) {
		write_log(ERROR, "WSAStartup failed with error: %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	HANDLE hMapFile = (HANDLE) strtoll(argv[1], NULL, 10);

	// open file mapping
	LPTSTR pBuf = (LPTSTR) MapViewOfFile(
			hMapFile, // handle to map object
			FILE_MAP_ALL_ACCESS,  // read/write permission
			0,
			0,
			sizeof(client_args)
	);
	if (pBuf == NULL) {
		write_log(ERROR, "MapViewOfFile failed wirh error: %d",
				GetLastError());
		exit(1);
	}

	// copying client_args into a local variable, so we can delete
	// the file mapping asap. This way, we dont risk it will never be closed
	// if the management_function fails
	client_args c;
	memcpy(&c, pBuf, sizeof(client_args));
	UnmapViewOfFile(pBuf);
	CloseHandle(hMapFile);

	management_function(&c);
	exit(1);
}
#endif
