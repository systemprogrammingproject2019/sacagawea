#ifdef _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sacagalib.h"

int main(int argc, char *argv[]){
	// chiamo load file in memory per testare se funzionava
	//load_file_memory_posix( "conf/sacagawea.conf");

	HANDLE hMapFile = strtol(argv[1], NULL, 10);
	LPTSTR pBuf = (LPTSTR) MapViewOfFile(
			hMapFile, // handle to map object
			FILE_MAP_ALL_ACCESS,  // read/write permission
			0,
			0,
			sizeof(client_args));


	if (pBuf == NULL) {
		write_log(ERROR, "MapViewOfFile failed wirh error: %d",
				GetLastError());
		exit(1);
	}
	client_args* c = calloc(1, sizeof(client_args));
	memcpy(c, pBuf, sizeof(client_args));
	UnmapViewOfFile(pBuf);
	CloseHandle(hMapFile);
	management_function(c);

	free(c);
	exit(1);
}
#endif
