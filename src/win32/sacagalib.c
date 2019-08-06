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
char MODE_CLIENT_PROCESSING = 0; // 0=thread 1=subProcess

// this function check if a line contain a new config
int check_if_conf(char line[]) {
	fprintf(stdout, S_LINE_READ_FROM_CONF_FILE, line);
	int port_change = false;
	// if line is type "mode [t/p]"
	if (strncmp(S_MODE, line, 4) == 0) {
		char mode;
		memcpy(&mode, &line[5], 1);
		if (mode == S_MODE_THREADED) {
			MODE_CLIENT_PROCESSING = 0;
		}
		if (mode == S_MODE_MULTIPROCESS) {
			MODE_CLIENT_PROCESSING = 1;
		}
		//fprintf(stdout,"mode change %c: %d\n", mode, MODE_CLIENT_PROCESSING);
	}

	// if line is "port XXX" with XXX a port number
	if (strncmp(S_PORT, line, 4) == 0) {
		long int val;
		val = strtol(&line[5], NULL, 10);

		if (val != SERVER_PORT) {
			SERVER_PORT = val;
			port_change = true;
		}
	}
	return port_change;
}

int listen_descriptor(SOCKET svr_socket) {
	SOCKET new_socket, s;
	int addrlen = sizeof(struct sockaddr_in), num_fd_ready;
	struct sockaddr_in address;
	struct client_args* client_info = malloc(sizeof(struct client_args));

	struct timeval timeout;
	timeout.tv_sec  = 13 * 60;
	timeout.tv_usec = 0;

	fd_set working_set;
	char str_client_addr[ADDR_MAXLEN];

	// /* create a copy of fds_set called working_set, is a FD_SET to work on  */
	memcpy(&working_set, &fds_set, sizeof(fds_set));

	// Prepare the socket set for network I/O notification
	FD_ZERO(&working_set);
	// Always look for connection attempts
	FD_SET(svr_socket, &working_set);

	//add child sockets to fd set
	for (int i = 0; i < MAX_CLIENTS; i++) {
		s = client_socket[i];
		if (s > 0) {
			FD_SET(s, &working_set);
		}
	}

	//wait for an activity on any of the sockets, timeout is NULL , so wait indefinitely
	if ((num_fd_ready = select(0, &working_set, NULL, NULL, &timeout)) == SOCKET_ERROR) {
		printf("select failed with error: %d\n", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i <= MAX_CLIENTS && num_fd_ready > 0; ++i) {

		// Check to see if the i-th descriptor is ready
		if (FD_ISSET(svr_socket, &working_set)) {
			/* if we come there, the descriptor is readable. */
			num_fd_ready -= 1;

			printf("Listening socket is readable \n");
			/* Accept all incoming connections that are queued up on the listening socket
				* before we loop back and call select again. */
			do {
				/*Accept each incoming connection.  If accept fails with EWOULDBLOCK,
				then we have accepted all of them.
				Any other failure on accept will cause us to end the server.  */
				memset(&address, 0, sizeof(address));
				size_t address_len = sizeof(address);
				if ((new_socket = accept(svr_socket, &address, &address_len)) == SOCKET_ERROR){
					printf("accept failed with error: %d\n", WSAGetLastError());
					break;
				}
				/* we create a t/p for management the incoming connection, call the right function with (socket , client_addr) as argument */
				snprintf(client_info->addr, ADDR_MAXLEN, "%s:%d", 
						inet_ntoa(address.sin_addr), address.sin_port);
				client_info->socket = new_socket;

				printf("New connection estabilished at fd - %d from %s\n",
						client_info->socket, client_info->addr);
				if (MODE_CLIENT_PROCESSING == 0) {
					thread_management(&client_info);
				} else {
					process_management(&client_info);
				}
			} while (new_socket != INVALID_SOCKET);
		}
	}
	return false;
}

// this fuction opens a listening socket
SOCKET open_socket() {
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
	if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) == SOCKET_ERROR) {
		printf("WSAStartup failed with error: %d\n", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family   = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags    = AI_PASSIVE;

	// windows requires the port to be a string
	char* port_to_string = malloc(16);
	snprintf(port_to_string, 15, "%d", SERVER_PORT);
	// Resolve the server address and port
	err = getaddrinfo(NULL, port_to_string, &hints, &result);
	if (err != 0) {
		printf("getaddrinfo failed: %d\n", err);
		WSACleanup();
		exit(EXIT_FAILURE);
	}
	free(port_to_string);

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %d", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	// Setup the TCP listening socket
	if ((bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen)) == SOCKET_ERROR) {
		printf("bind failed with error: %d", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result);

	if ((listen(ListenSocket, SOMAXCONN)) == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	// Change the socket mode on the listening socket from blocking to
	// non-block so the application will not block waiting for requests
	ULONG NonBlock = true;
	if (ioctlsocket(ListenSocket, FIONBIO, &NonBlock) == SOCKET_ERROR){
		printf("ioctlsocket failed with error %d\n", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	return ListenSocket;
}

int process_management(struct client_args *client_info) {
	return false;
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
	if (fp == NULL) {
		fprintf(stderr, S_ERROR_FOPEN, strerror(errno));
	 	exit(5);
	}

	//readline or 100 char
	do {
		if (fgets(line, max_line_size, fp) == NULL) {
			if (feof(fp)) {
				keep_going = false;
			} else {
				fprintf(stderr, S_ERROR_FGETS, strerror(errno));
				exit(5);
			}
		}
		// check if the line is a config line
		if ((strlen(line) != 100) && (check_if_conf(line))) {
			port_change = true;
		}
	} while (keep_going);

	return port_change;
}

int thread_management(struct client_args *client_info) {
	return false;
}
