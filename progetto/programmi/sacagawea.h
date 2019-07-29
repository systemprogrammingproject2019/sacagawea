#define true     1
#define false    0
#define PATH_MAX        4096 // in Linux the max path is 4096 chars
#define SACAGAWEACONF_PATH "conf/sacagawea.conf"

int SERVER_PORT=7070;
int MODE_CLIENT_PROCESSING=0; // 0=thread 1=subProcess
int SERVER_SOCKET; // the socket descriptor of the server
fd_set fds_set;
int max_num_s;


void open_socket();
int check_if_conf(char line[]);
int read_and_check_conf();
void config_handler(int signum);
int listen_descriptor();
int load_file_memory_linux( char *path);
int load_file_memory_posix( char *path);

