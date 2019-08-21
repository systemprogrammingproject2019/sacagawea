#ifndef SACAGALIB_H
#define SACAGALIB_H

#ifdef _WIN32
#include <winsock2.h> // needed for 'SOCKET' type
#endif

#include "exported.h"

#define ERROR   0
#define WARNING 1
#define INFO    2
#define DEBUG   3

#ifndef LOG_LEVEL
#define LOG_LEVEL INFO
#endif

#define true    1
#define false   0
#define SACAGAWEACONF_PATH "sacagawea.conf"
#define SACAGAWEALOGS_PATH "sacagawea.log"
#define DEFAULT_SERVER_PORT 7070
#define MAX_CLIENTS 64
#define SERVER_DOMAIN "michele-pc"

// max length of IP is 15 254.254.254.254 + 1 char for ':'
// + 5 char for port 65000 + 1 char for the terminating null byte
#define ADDR_MAXLEN 22

#define MAX_FILE_NAME 255 // in Linux the max file name is 255 bytes
#ifdef _WIN32
// under Win32 the PATH_MAX and MAX_PATH are already defined
// their length is 260 chars
#else
#define PATH_MAX 4096 // in Linux the max path is 4096 chars
#endif

// windows needs it as a number, linux as a string
#define SOCK_RECVBUF_LEN   65536
#define S_SOCK_RECVBUF_LEN "65536"

int SERVER_PORT;
char MODE_CLIENT_PROCESSING;
fd_set fds_set;

#ifndef _WIN32
int max_num_s;

int pipe_conf[2];
pthread_cond_t *cond;
pthread_mutex_t *mutex;
#endif

struct struct_client_args {
		char client_addr[ADDR_MAXLEN];
		int socket;
		char *path_file; // is the path of file in the server ROOT_PATH + SELECTOR
		char *file_to_send;
		long len_file;
};

typedef struct struct_client_args client_args;

struct struct_selector {
		char selector[PATH_MAX];
		int num_words;
		char **words;
};

typedef struct struct_selector selector;


// children_management.c
EXPORTED selector request_to_selector(char *input);
EXPORTED void *thread_sender(void* c);
EXPORTED void *thread_function(void* c);
EXPORTED int thread_management(client_args *client_info);
EXPORTED int process_management(client_args *client_info);

// socket.c
#ifdef _WIN32
SOCKET client_socket[MAX_CLIENTS];
SOCKET SERVER_SOCKET;        // the server socket's handle
EXPORTED SOCKET open_socket();
EXPORTED int listen_descriptor(SOCKET);
#else
int SERVER_SOCKET; // the server socket's file descriptor
int open_socket();
int listen_descriptor();
#endif

EXPORTED int check_security_path();
EXPORTED int load_file_memory_linux(char *path);

// config.c
EXPORTED int check_if_conf(char line[]);
EXPORTED int read_and_check_conf();
EXPORTED void config_handler(int signum);

// gopher.c
EXPORTED char type_path(char path[PATH_MAX]);

// log_management.c
EXPORTED void log_management();

#define SHARED_MUTEX_MEM "/shared_memory_for_mutex"
#define SHARED_COND_MEM "/shared_memory_for_cond"

/* Universal strings */
#define S_LINE_READ_FROM_CONF_FILE "Line read from conf file: %s"
#define S_MODE              "mode"
#define S_MODE_THREADED     't'
#define S_MODE_MULTIPROCESS 'p'
#define S_PORT "port"
#define S_ERROR_FOPEN "fopen() failed: %s\n"
#define S_ERROR_FGETS "fgets() failed: %s\n"

// all "file -bi file_path" command output
#ifdef _WIN32
#define S_ROOT_PATH ".\\"
#else
#define S_ROOT_PATH "./"
#endif
#define TEXT_0         "text/" // vale per .txt .conf .c .py ...
#define HTML_h         "text/html"
#define GIF_g          "image/gif"
#define IMAGE_I        "image/" // vale per .jpg
#define DIR_1          "inode/directory"
#define EMPTY_0        "inode/x-empty" // file empty, we use 0 for defoult
#define GOPHER_1       "application/gopher"
#define MAC_4          "application/mac"
#define APPLICATION_9  "application/"
#define AUDIO_s        "audio/"
#define MULTIPART_M    "multipart/mixed"

#endif
