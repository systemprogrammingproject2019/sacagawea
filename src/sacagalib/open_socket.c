#ifdef _WIN32
#include <winsock2.h> // needed for 'SOCKET' type
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#endif

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h> // for close

#include "sacagalib.h"

#ifdef _WIN32
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
#else
// this fuction opens a listening socket.
int open_socket(){
	int on=1;
	struct sockaddr_in serv_addr;
	/*The socket() API returns a socket descriptor, which represents an endpoint.
		The statement also identifies that the INET (Internet Protocol) 
		address family with the TCP transport (SOCK_STREAM) is used for this socket.*/
	if ( (SERVER_SOCKET = socket(AF_INET, SOCK_STREAM, 0)) < 0 ){
		fprintf( stderr,"socket failed: %s\n", strerror(errno));
	 	exit(5);
	}

	/*The ioctl() API allows the local address to be reused when the server is restarted 
	before the required wait time expires. In this case, it sets the socket to be nonblocking. 
	All of the sockets for the incoming connections are also nonblocking because they inherit that state from the listening socket. */
	if ( (ioctl(SERVER_SOCKET, FIONBIO, (char *)&on)) < 0 ){
		fprintf( stderr,"ioctl failed: %s\n", strerror(errno));
		close(SERVER_SOCKET);
		exit(5);
	}

	/* Set max recvbuf to match windows version's */
	if (setsockopt(SERVER_SOCKET, SOL_SOCKET, SO_RCVBUF, S_SOCK_RECVBUF_LEN, sizeof(S_SOCK_RECVBUF_LEN))) {
		fprintf(stderr, "setsockopt failed: %s\n", strerror(errno));
		exit(5);
	}

	/*declare sockaddr_in */
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons( SERVER_PORT );

	// bind to join the unamed socket with sockaddr_in and become named socket
	if( bind( SERVER_SOCKET , (struct sockaddr*)&serv_addr ,  sizeof(serv_addr)) == -1 ){
		fprintf( stderr,"bind failed: %s\n", strerror(errno) );
		exit(5);
	}

	/* listen allows the server to accept incoming client connection  */
	if ( (listen( SERVER_SOCKET, 32)) < 0){
		fprintf( stderr,"listen failed: %s\n", strerror(errno) );
		exit(5);
	}
	return SERVER_SOCKET;
}
#endif
