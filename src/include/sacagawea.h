#ifndef SACAGAWEA_H
#define SACAGAWEA_H

#ifdef _WIN32
#include <winsock2.h> // needed for 'SOCKET' type
#endif

/* This is needed in order to export functions to the DLL
 * (Linux doesn't need an equivalent for the .so file because)
 * all the functions are imported by default
 */
#ifdef _WIN32
# ifdef WIN_EXPORT
#   define EXPORTED  __declspec(dllexport)
# else
#   define EXPORTED  __declspec(dllimport)
# endif
#else
# define EXPORTED
#endif

#define true     1
#define false    0
#define PATH_MAX 4096 // in Linux the max path is 4096 chars
#define MAX_FILE_NAME 255 // in Linux the max file name is 255 bytes
#define SACAGAWEACONF_PATH "sacagawea.conf"
#define DEFAULT_SERVER_PORT 7070
#define MAX_CLIENTS 64

// max length of IP is 15 254.254.254.254 + 1 char for ':'
// + 5 char for port 65000 + 1 char for the terminating null byte
#define ADDR_MAXLEN 22

// windows needs it as a number, linux as a string
#define SOCK_RECVBUF_LEN   65536
#define S_SOCK_RECVBUF_LEN "65536"

extern int SERVER_PORT;
extern char MODE_CLIENT_PROCESSING;
fd_set fds_set;

#ifdef _WIN32
SOCKET client_socket[MAX_CLIENTS];
SOCKET SERVER_SOCKET;        // the server socket's handle
EXPORTED SOCKET open_socket();
EXPORTED int listen_descriptor(SOCKET);
#else
int SERVER_SOCKET; // the server socket's file descriptor
int open_socket();
int listen_descriptor(int);
#endif

EXPORTED int check_if_conf(char line[]);
EXPORTED int read_and_check_conf();
EXPORTED int check_security_path();

/* Universal strings */
#define S_LINE_READ_FROM_CONF_FILE "Line read from conf file: %s"
#define S_MODE              "mode"
#define S_MODE_THREADED     't'
#define S_MODE_MULTIPROCESS 'p'
#define S_PORT "port"
#define S_ERROR_FOPEN "fopen() failed: %s\n"
#define S_ERROR_FGETS "fgets() failed: %s\n"


// all "file -bi file_path" command output
#define S_ROOT_PATH "./"
#define TEXT_0 "text/" // vale per .txt .conf .c .py ...
#define HTML_h "text/html"
#define GIF_g "image/gif"
#define IMAGE_I "image/" // vale per .jpg
#define DIR_1 "inode/directory"
#define EMPTY_0 "empty" // file empty, we use 0 for defoult
#define GOPHER_1 "application/gopher"
#define MAC_4 "application/mac"
#define APPLICATION_9 "application/"
#define AUDIO_s "audio/"
#define MULTIPART_M "multipart/mixed"

#endif