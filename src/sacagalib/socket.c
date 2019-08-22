#ifdef _WIN32
#include <winsock2.h> // needed for 'SOCKET' type
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#endif

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> // for close

#include "sacagalib.h"

int SERVER_PORT = DEFAULT_SERVER_PORT;
char MODE_CLIENT_PROCESSING = (char) 0; // 0 = thread --- 1 = subProcess

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
// this fuction opens a listening socket. In LINUX 1.0+
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

#ifdef _WIN32
int listen_descriptor(SOCKET svr_socket) {
	SOCKET new_socket, s;
	int addrlen = sizeof(struct sockaddr_in), num_fd_ready;
	struct sockaddr_in address;
	#ifdef _WIN32
	client_args* client_info = malloc(sizeof(client_args));
	#else
	struct client_args* client_info = malloc(sizeof(struct client_args));
	#endif

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

			if (i==SERVER_SOCKET) {
				printf("\n--------------------\nListening socket is readable\n--------------------\n\n");
				/* Accept all incoming connections that are queued up on the listening socket
					* before we loop back and call select again. */
				do {
					/*Accept each incoming connection.  If accept fails with EWOULDBLOCK,
					then we have accepted all of them.
					Any other failure on accept will cause us to end the server.  */
					memset(&address, 0, sizeof(address));
					size_t address_len = sizeof(address);
					if ((new_socket = accept(svr_socket, &address, &address_len)) == SOCKET_ERROR){
						int e = WSAGetLastError();
						if (e != WSAEWOULDBLOCK) {
							fprintf(stderr, "accept failed with error: %d\n", e);
						}
						break;
					}
					/* we create a t/p for management the incoming connection, call the right function with (socket , client_addr) as argument */
					snprintf(client_info->client_addr, ADDR_MAXLEN, "%s:%d", 
							inet_ntoa(address.sin_addr), address.sin_port);
					client_info->socket = new_socket;

					printf("New connection estabilished at socket - %d from %s\n",
							client_info->socket, client_info->client_addr);
					if (MODE_CLIENT_PROCESSING == 0) {
						thread_management(&client_info);
					} else {
						if (MODE_CLIENT_PROCESSING == 1){
							process_management(client_info);
						}else{
							fprintf( stderr,"WRONG MODE PLS CHECK: %d\n", MODE_CLIENT_PROCESSING );
							exit(5);
						}
					}
				} while (new_socket != INVALID_SOCKET);
			}
		}
	}
	return false;
}
#else
// this function call the select() and check the FDS_SET if some socket is readable
int listen_descriptor() {
	// Some declaretion of usefull variable
	struct sockaddr_in client_addr;
	int i, num_fd_ready, check, client_addr_len;
	struct timeval timeout;
	fd_set working_set;
	int close_conn;
	//char str_client_addr[ (12+3+1+5) ]; // max lenght of IP is 16 254.254.254.254 + 5 char for port 65000
	int new_s;
	// struct defined in sacagawea.h for contain client information
	client_args *client_info;
	client_info = (client_args*) malloc( sizeof(client_args));
	memset( client_info, 0, sizeof(client_info));
	/* Initialize the timeval struct to 13 minutes.  If no        
	activity after 13 minutes this program will end.           */
	timeout.tv_sec  = 13 * 60;
	timeout.tv_usec = 0;

	/* create a copy of fds_set called working_set, is a FD_SET to work on  */
	memcpy( &working_set, &fds_set, sizeof(fds_set));

	// start select and check if failed
	printf("Waiting on select()...\n");
	check = select( max_num_s+1, &working_set, NULL, NULL, &timeout);
	/* if errno==EINTR the select is interrupted becouse of sigaction 
	so we have to repeat select, not exit(5) */
	if ( (check < 0) && (errno != EINTR) ){
		fprintf( stderr,"select() failed: %s\n", strerror(errno) );
		exit(5);
	}// Chek if select timed out
	if (check == 0){
		printf("select() timed out. End program.\n");
		return true;
	}
	/* 1 or more descriptors are readable we have to check which they are */
	num_fd_ready=check;
	// for, for check all ready FD in fds_set until, FD are finish or we check all the ready fd


	// qui ci sta un for perche avevo fatto una versione differente prima , devo sistemare don't worry
	// in realta il for non serve perche prima inserivo anche le nuove connessioni dentro FD_SET cosi
	// creavo il thread/processo solo quando era effettivamente leggibile, ma non cambiava nulla anzi
	// mi complicavo la vita a dover creare un dizionario per salvarmi informazioni ecc... dopo lo sistemo
	for (i=0;  i <= max_num_s && num_fd_ready > 0; ++i){
		close_conn = false;
		// Check to see if the i-esimo descriptor is ready
		if (FD_ISSET(i, &working_set)){
			/* if we come there, the descriptor is readable. */
			num_fd_ready -= 1;

			if (i==SERVER_SOCKET){
				printf("\n--------------------\nListening socket is readable\n--------------------\n\n");
				/*Accept all incoming connections that are queued up on the listening socket before we
				loop back and call select again. */
				do{
					/*Accept each incoming connection.  If accept fails with EWOULDBLOCK,
					then we have accepted all of them.
					Any other failure on accept will cause us to end the server.  */
					memset(&client_addr, 0, sizeof(client_addr));
					client_addr_len = sizeof(client_addr); // save sizeof sockaddr struct becouse accept need it
					new_s = accept(SERVER_SOCKET, &client_addr, &client_addr_len);
					if (new_s < 0){
						if (errno != EWOULDBLOCK){
							fprintf( stderr,"socket accept() failed: %s\n", strerror(errno) );
							exit(5);
						}
						break;
					}
					/* we create a t/p for management the incoming connection, call the right function with (socket , client_addr) as argument */
					snprintf( client_info->client_addr, ADDR_MAXLEN, "%s:%d", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
					printf("New connection stabilished at fd - %d from %s\n", new_s, client_info->client_addr);
					client_info->socket = new_s;

					if ( MODE_CLIENT_PROCESSING == 0){
						thread_management( client_info );
					}else{
						if ( MODE_CLIENT_PROCESSING == 1){
							process_management( client_info );
						}else{
							fprintf( stderr,"WRONG MODE PLS CHECK: %d\n", MODE_CLIENT_PROCESSING );
							exit(5);
						}
					}

				}while (new_s != -1);
					
			}
		} // End of select loop
	}
	return false; 
}
#endif
