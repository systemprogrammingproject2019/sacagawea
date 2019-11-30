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

int accept_wrapper(const settings_t*);

// this fuction opens a listening socket
sock_t open_socket(const settings_t* settings) {
#ifdef _WIN32
	WSADATA wsaData;
	int err;

	sock_t ListenSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	// Initialize Winsock
	if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) == SOCKET_ERROR) {
		write_log(ERROR, "WSAStartup failed with error: %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family   = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags    = AI_PASSIVE;

	// windows requires the port to be a string,
	// because it actually is the "service name"
	char* port_to_string = malloc(16);
	snprintf(port_to_string, 15, "%d", settings->port);
	// Resolve the server address and service (port)
	err = getaddrinfo(NULL, port_to_string, &hints, &result);
	if (err != 0) {
		write_log(ERROR, "getaddrinfo failed: %d", err);
		WSACleanup();
		exit(EXIT_FAILURE);
	}
	free(port_to_string);

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		write_log(ERROR, "socket failed with error: %d", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	// Change the socket mode on the listening socket from blocking to
	// non-block so the application will not block waiting for requests
	ULONG NonBlock = true;
	if (ioctlsocket(ListenSocket, FIONBIO, &NonBlock) == SOCKET_ERROR) {
		write_log(ERROR, "ioctlsocket failed with error %d\n", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	if (setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEADDR, &(char){true}, sizeof(int)) != 0) {
		write_log(ERROR, "setsockopt(SO_REUSEADDR) failed with error: %d", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	// Setup the TCP listening socket
	if ((bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen)) == SOCKET_ERROR) {
		write_log(ERROR, "bind failed with error: %d", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result);

	if ((listen(ListenSocket, SOMAXCONN)) == SOCKET_ERROR) {
		write_log(ERROR, "listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	return ListenSocket;

#else
	sock_t ListenSocket;
	int on = 1;
	struct sockaddr_in serv_addr;
	/*The socket() API returns a socket descriptor, which represents an endpoint.
		The statement also identifies that the INET (Internet Protocol) 
		address family with the TCP transport (SOCK_STREAM) is used for this socket.*/
	if ((ListenSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
		write_log(ERROR, "socket failed: %s", strerror(errno));
		exit(5);
	}

	if (setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
		write_log(ERROR, "setsockopt(SO_REUSEADDR) failed: %s", strerror(errno));
	}

#ifdef SO_REUSEPORT
	if (setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) < 0) {
		write_log(ERROR, "setsockopt(SO_REUSEPORT) failed: %s", strerror(errno));
	}
#endif

	/*The ioctl() API allows the local address to be reused when the server is restarted 
	before the required wait time expires. In this case, it sets the socket to be nonblocking. 
	All of the sockets for the incoming connections are also nonblocking because they inherit that state from the listening socket. */
	if ((ioctl(ListenSocket, FIONBIO, (char *)&on)) < 0) {
		write_log(ERROR, "ioctl failed: %s", strerror(errno));
		close(ListenSocket);
		exit(5);
	}

	/* Set max recvbuf to match windows version's */
	if (setsockopt(ListenSocket, SOL_SOCKET, SO_RCVBUF, S_SOCK_RECVBUF_LEN, sizeof(S_SOCK_RECVBUF_LEN))) {
		write_log(ERROR, "setsockopt failed: %s", strerror(errno));
		exit(5);
	}

	/*declare sockaddr_in */
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(settings->port);

	// bind to join the unamed socket with sockaddr_in and become named socket
	if (bind(ListenSocket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
		write_log(ERROR, "bind failed: %s\n", strerror(errno));
		exit(5);
	}

	/* listen allows the server to accept incoming client connection  */
	if ((listen(ListenSocket, 32)) < 0) {
		write_log(ERROR, "listen failed: %s\n", strerror(errno));
		exit(5);
	}
	return ListenSocket;
#endif
}

int listen_descriptor(const settings_t* settings) {
	int num_fd_ready;//, addrlen = sizeof(struct sockaddr_in);

	num_fd_ready = 0;
	// Prepare the socket set for network I/O notification
	FD_ZERO(&fds_set);
	// Always look for connection attempts
	FD_SET(settings->socket, &fds_set);

	// start select and check if failed
	write_log(INFO, "Waiting on select()...");

	// we only need to monitor the settings->socket, so the first arg of select
	// can just be "settings->socket + 1", which is the highest number of fd
	// we need to monitor
	num_fd_ready = select(settings->socket + 1, &fds_set, NULL, NULL, 
#ifdef _WIN32		
			&(struct timeval){1, 0}); // 1 second timeout
#else
			NULL);
#endif

#ifdef _WIN32
	if (num_fd_ready == SOCKET_ERROR) {
		write_log(ERROR, "select failed with error: %d", WSAGetLastError());
		exit(1);
	}
#else
	// if select returns a number lesser than 0, an error occurred
	if (num_fd_ready < 0) {
		/* if errno == EINTR the select is interrupted becouse of sigaction 
		so we have to repeat select, not exit(5) */
		if (errno == EINTR) {
			return true;
		}
		write_log(ERROR, "select failed: %s", strerror(errno));
		exit(1);
	}
#endif
	else {
		if (num_fd_ready == 0) {
			return true;
		}
	}

	if (FD_ISSET(settings->socket, &fds_set)) {
		printf("\n--------------------\nListening socket is readable\n--------------------\n\n");
		/*Accept all incoming connections that are queued up on the listening
		socket before we loop back and call select again. */
		accept_wrapper(settings);
	}
	return true; 
}


int accept_wrapper(const settings_t* settings) {
	int new_s;
	struct sockaddr_in addr;

	/*Accept each incoming connection.  If accept fails with EWOULDBLOCK,
	then we have accepted all of them.
	Any other failure on accept will cause us to end the server.  */
	socklen_t addr_len = sizeof(struct sockaddr_in); // save sizeof sockaddr struct becouse accept need it
	memset(&addr, 0, (size_t) addr_len);

	while (true) {
		client_args* client_info = (client_args*) calloc(1, sizeof(client_args));
		memcpy(&(client_info->settings), settings, sizeof(settings_t));
		// accept the connected connection which already did 3WHS.
		new_s = accept(settings->socket, (struct sockaddr*) &addr, &addr_len);

	#ifdef _WIN32
		if (new_s == INVALID_SOCKET) {
			free(client_info);
			break;
		}
	#else
		if (new_s == -1) {
			free(client_info);
			break;
		}
	#endif

	#ifdef _WIN32
		// Change the socket mode on the accepting socket from non-blocking to
		// blocking so the application will be able to have blocking sends
		unsigned long NonBlock = false;
		if (ioctlsocket(new_s, FIONBIO, &NonBlock) == SOCKET_ERROR) {
			write_log(ERROR, "ioctlsocket failed with error %d\n",
					WSAGetLastError());
			closesocket(new_s);
			exit(EXIT_FAILURE);
		}
	#else
		// in Linux, all sockets created with accept are BLOCKING by default
	#endif

		if (new_s < 0) {
		#ifdef _WIN32
			if (WSAGetLastError() != WSAEWOULDBLOCK) {
				write_log(ERROR, "socket accept() failed with error: %d", WSAGetLastError());
				free(client_info);
				exit(5);
			}
		#else
			if (errno != EWOULDBLOCK) {
				write_log(ERROR, "socket accept() failed: %s", strerror(errno) );
				free(client_info);
				exit(5);
			}
		#endif
		}
		/* we create a t/p for management the incoming connection, call the right function with (socket , addr) as argument */
		snprintf(client_info->addr, ADDR_MAXLEN, "%s:%d", inet_ntoa(addr.sin_addr), addr.sin_port);
		write_log(INFO, "New connection estabilished at fd - %d from %s", new_s, client_info->addr);
		client_info->socket = new_s;

		if (settings->mode == 't') {
			thread_management(client_info);
		} else {
			if (settings->mode == 'p') {
				process_management(client_info);
			} else {
				write_log(ERROR, "Multithread/multiprocess mode not set correctly");
				free(client_info);
				exit(5);
			}
		}
	}
	return true;
}
