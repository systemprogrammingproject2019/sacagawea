#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "sacagawea.h"
#include "win32/sacagalib.h"

int SERVER_PORT = DEFAULT_SERVER_PORT;

// this function check if a line contain a new config
int check_if_conf(char line[]) {
	fprintf(stdout, S_LINE_READ_FROM_CONF_FILE, line);
	int port_change = false;
	// if line is type "mode [t/p]"
	if(strncmp(S_MODE, line, 4) == 0) {
		char mode;
		memcpy(&mode, &line[5], 1);
		if(mode == S_MODE_THREADED) {
			MODE_CLIENT_PROCESSING = 0;
		}
		if(mode == S_MODE_MULTIPROCESS) {
			MODE_CLIENT_PROCESSING = 1;
		}
		//fprintf(stdout,"mode change %c: %d\n", mode, MODE_CLIENT_PROCESSING);
	}

	// if line is "port XXX" with XXX a port number
	if(strncmp(S_PORT, line, 4) == 0) {
		long int val;
		val = strtol(&line[5], NULL, 10);

		if(val != SERVER_PORT) {
			SERVER_PORT = val;
			port_change = true;
		}
	}
	return port_change;
}

// this fuction opens a listening socket
void open_socket() {
	WSADATA wsaData;
	int err;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int iSendResult;
	char recvbuf[SOCK_RECVBUF_LEN];
	int recvbuflen = SOCK_RECVBUF_LEN;

	// Initialize Winsock
	if ((WSAStartup(MAKEWORD(2,2), &wsaData)) != 0) {
		printf("WSAStartup failed: %s\n", strerror(errno));
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// windows requires the port to be a string
	char* port_to_string = malloc(16);
	snprintf(port_to_string, 15, "%d", SERVER_PORT);
	// Resolve the server address and port
	err = getaddrinfo(NULL, port_to_string, &hints, &result);
	if (err != 0) {
		printf("getaddrinfo failed: %s\n", err);
		WSACleanup();
		return 1;
	}
	free(port_to_string);

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	if ((bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen)) == SOCKET_ERROR) {
		printf("bind failed with error: %d", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	if ((listen(ListenSocket, SOMAXCONN)) == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// // Accept a client socket
	// ClientSocket = accept(ListenSocket, NULL, NULL);
	// if (ClientSocket == INVALID_SOCKET) {
	// 	printf("accept failed with error: %d\n", WSAGetLastError());
	// 	closesocket(ListenSocket);
	// 	WSACleanup();
	// 	return 1;
	// }

	// // No longer need server socket
	// closesocket(ListenSocket);
}

// this function read the sacagawea.conf line by line 
int read_and_check_conf() {
	// some declaretion 
	FILE *fp;
	const size_t max_line_size = 100;
	char line[max_line_size];
	int keep_going = true;
	int port_change = false;
	//open config file and check if an error occured
	fp = fopen(SACAGAWEACONF_PATH, "r");
	if(fp == NULL) {
		fprintf(stderr, S_ERROR_FOPEN, strerror(errno));
	 	exit(5);
	}

	//readline or 100 char
	do {
		if(fgets(line, max_line_size, fp) == NULL) {
			if(feof(fp)) {
				keep_going = false;
			} else {
				fprintf(stderr, S_ERROR_FGETS, strerror(errno));
				exit(5);
			}
		}
		// check if the line is a config line
		if((strlen(line) != 100) && (check_if_conf(line))) {
			port_change = true;
		}
	} while(keep_going);

	return port_change;
}

