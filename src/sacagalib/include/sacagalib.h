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
#define LOG_LEVEL DEBUG
#endif

// need these for separating higher and lower bits for "File Mapping" functions
#define LODWORD(l) ((DWORD) (((DWORDLONG) (l)) & 0xffffffff))
#define HIDWORD(l) ((DWORD) ((((DWORDLONG) (l)) >> 32) & 0xffffffff))

#define true     1
#define false    0
#define SACAGAWEACONF_PATH "sacagawea.conf"
#define SACAGAWEALOGS_PATH "sacagawea.log"
#define DEFAULT_SERVER_PORT 7070
#define MAX_CLIENTS 64
// #define SERVER_DOMAIN "localhost"

#define WIN32_PIPE_NAME "\\\\.\\pipe\\logger"
#define WIN32_PIPE_BUFSIZE 1024
#define WIN32_MAX_PIPES 64

#ifdef _WIN32
#define sock_t SOCKET
#define thread_t HANDLE
#else
#define sock_t int
#define thread_t int
#endif

// max length of IP is 15 254.254.254.254 + 1 char for ':'
// + 5 char for port 65000 + 1 char for the terminating null byte
#define ADDR_MAXLEN 22
#define MAX_FILE_NAME 255 // in Linux the max file name is 255 bytes
#define MAX_REQUEST_LEN  4096 // we can change that with logs


#ifdef _WIN32
// under Win32 the PATH_MAX and MAX_PATH are already defined
// their length is 260 chars
#else
#define PATH_MAX 4096 // in Linux the max path is 4096 chars
#endif

// windows needs it as a number, linux as a string
#define SOCK_RECVBUF_LEN   65536
#define S_SOCK_RECVBUF_LEN "65536"

// this struct contains all the server's settings
typedef struct struct_settings_t {
	int port;
	sock_t socket;
	char mode;
	char homedir[PATH_MAX];
	char hostname[256];
} settings_t;

extern settings_t* settings;

fd_set fds_set;

int logs_proces_pid;
#ifndef _WIN32

int max_num_s;

// struct used for management "Condition variable"
typedef struct struct_conditionVariable{
	int pipe_conf[2];
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	int cont; // if log process wakes up and finds the associated mutex is still not available,
	// it sleeps again. So he lose 1 signal, for that we cont the number of signal, so that
	// when is waked up from another signal, the log process remedies the previously lost signal.
} conditionVariable;

conditionVariable* condVar;


#endif

struct struct_client_args{
	char    addr[ADDR_MAXLEN];
	sock_t  socket;
	char    *path_file; // is the path of file in the server ROOT_PATH + SELECTOR
	
	// on win32 its a handle to a file mapping,
	// whereas on linux its the address to the mapped file
#ifdef _WIN32
	HANDLE file_to_send;
#else
	char *file_to_send;
#endif
	size_t len_file;
	settings_t settings;
};

typedef struct struct_client_args client_args;


// sacagalib.c
EXPORTED void print_client_args(client_args* client);

// children_management.c
EXPORTED long unsigned int *management_function(client_args* c);
// EXPORTED void process_fuction(client_args *client_info);
EXPORTED void thread_management(client_args *client_info);
EXPORTED int process_management(client_args *client_info);
EXPORTED void free_client_args(client_args*);

// the name is self explanatory
void close_socket_kill_child(client_args* c, int errcode);

// socket.c
EXPORTED sock_t open_socket(const settings_t* settings);
#ifdef _WIN32
sock_t client_socket[MAX_CLIENTS];
#endif
EXPORTED int listen_descriptor(const settings_t*);

EXPORTED int check_security_path();
EXPORTED int load_file_memory_linux(char *path);

// config.c
void read_conf_line(const char* line, settings_t* settings);
int check_if_port_change(const char* line, settings_t*);
EXPORTED char* do_regex(const char* pattern, const char* str);
EXPORTED int read_and_check_conf(settings_t*, int);

// gopher.c
EXPORTED char type_path(char* path);
EXPORTED void *thread_sender(client_args* c);
EXPORTED void send_content_of_dir(client_args *client_info, char *client_selector);
#ifndef _WIN32
EXPORTED char* sanitize_path(const char* input);
#endif

// log.c
EXPORTED void log_management();
EXPORTED void write_log(int, const char*, ...);
EXPORTED char* date_string();

// utility.c
EXPORTED int load_file_memory_and_send(client_args *client_info);
EXPORTED int check_security_path(char path[PATH_MAX]);


#define SHARED_COND_VARIABLE_MEM "/shared_memory_for_conditionVariable"

/* Universal strings */
#define S_LINE_READ_FROM_CONF_FILE "Line read from conf file: %s"
#define S_MODE              "mode"
#define S_MODE_THREADED     't'
#define S_MODE_MULTIPROCESS 'p'
#define S_PORT              "port"
#define S_HOMEDIR           "home-directory"
#define S_HOSTNAME          "hostname"

#define S_ERROR_FOPEN "fopen() failed: %s"
#define S_ERROR_FGETS "fgets() failed: %s"

// all "file -bi file_path" command output
#ifdef _WIN32
#define S_ROOT_PATH ""
#else
#define S_ROOT_PATH "./"
#endif
#define TEXT_0 "text/" // vale per .txt .conf .c .py ...
#define HTML_h "text/html"
#define GIF_g "image/gif"
#define IMAGE_I "image/" // vale per .jpg
#define DIR_1 "inode/directory"
#define EMPTY_0 "inode/x-empty" // file empty, we use 0 for defoult
#define GOPHER_1 "application/gopher"
#define MAC_4 "application/mac"
#define APPLICATION_9 "application/"
#define AUDIO_s "audio/"
#define MULTIPART_M "multipart/mixed"

#endif
