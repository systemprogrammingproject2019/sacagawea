#ifndef SACAGALIB_H
#define SACAGALIB_H

int max_num_s;

int pipe_conf[2];
pthread_cond_t *cond;
pthread_mutex_t *mutex;

void config_handler(int signum);
int load_file_memory_linux(char *path);

void log_management();

#define MAX_FILE_NAME 255 // in Linux the max file name is 255 bytes
#define PATH_MAX 4096 // in Linux the max path is 4096 chars

#define SHARED_MUTEX_MEM "/shared_memory_for_mutex"
#define SHARED_COND_MEM "/shared_memory_for_cond"

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