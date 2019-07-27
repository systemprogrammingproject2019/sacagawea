#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <getopt.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>


#define true     1
#define false    0
#define PATH_MAX        4096 // in Linux the max path is 4096 chars

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
/*
if command line is wrong the program will exit with status 20
if a system call failed the program exit with status 5
*/


int load_file_memory_posix( char *path)
{
  
  // open get file descriptor associated to file
  int fd = open ( path , O_RDWR );
  if ( fd < 0 )
  { 
    fprintf( stderr,"System call fdopen() failed because of %s", strerror(errno));
   	exit(5);
  }
  // declare struct for 3th argument for fcntl and memset it to 0
  struct flock lck;
  if( memset(&lck, 0, sizeof(lck)) == NULL )
  { 
    fprintf( stderr,"System call memset() failed because of %s", strerror(errno));
   	exit(5);
  }

  // F_WRLCK mean exclusive lock and not shared lock
  /* difference, first put lock for read and write, in the second one
  process if another is reading from the file can read simultanealy 
  but cant write, and if 1 is writing no other one can write or read */
  lck.l_type = F_WRLCK;
  // lock entire file
  lck.l_whence = SEEK_SET; // offset base is start of the file "SEEK_END mean start at end of file"
  lck.l_start = 0;         // starting offset is zero
  lck.l_len = 0;           // len is zero, which is a special value representing end
                          // of file (no matter how large the file grows in future)
  lck.l_pid = getppid(); // process holding the lock, we use PPID for all file lock

  /* this version use SETLKW with associed lock at couple [i-node,process], so threads share the lock
  but forked process nope, becouse they have differend PID. But all have the same DAD the PPID we use that
  for declare a only lock for file. */
  fcntl (fd, F_SETLKW, &lck);
  // now we have the lock "load file in memory"

  /* initialize the memory for load the file, 
  fseek put the FP at END ftell say the position ( file size ), we come back at start with SEEK_SET*/
  FILE* fp = fdopen(fd, "r");
  if ( fp == NULL )
  { 
    fprintf( stderr,"System call fdopen() failed because of %s", strerror(errno));
   	exit(5);
  }
  fseek(fp, 0, SEEK_END);
  long len = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *file_content = malloc( (len+1) );
  fread( file_content, 1, len, fp);
  file_content[len]= '\0';
  
  // we check the tipe or file with file bash command
  /* char command[ (strlen(path))+5 ]; 
  command = strcat( "file", path );
  FILE* popen_output_stream = popen( command , "r" )
  if ( popen_output_stream == NULL )
  { 
    fprintf( stderr,"System call popen() failed because of %s", strerror(errno));
   	exit(5);
  }
  char* popen_output = malloc( Pat );
  while ( fgets(popen_output, 100, popen_output_stream) != NULL)
    printf("%s", path); */

  // release lock with F_UNLCK flag and FP FD
  fclose(fp);
  close(fd);
  lck.l_type = F_UNLCK;
  fcntl (fd, F_SETLK, &lck);

  // qui generare thread che spedisce il file al momento lo stampo
  fprintf( stdout , "file: %s", file_content);
  free(file_content);
}

// VERSIONE LINUX, NON POSIX dovremmo chiedere al prof se si può usare ma non credo
int load_file_memory_linux( char *path)
{
  // open get file descriptor associated to file
  int fd = open ( path , O_RDWR );
  if ( fd < 0 )
  { 
    fprintf( stderr,"System call fdopen() failed because of %s", strerror(errno));
   	exit(5);
  }
  // declare struct for 3th argument for fcntl and memset it to 0
  struct flock lck;
  if( memset(&lck, 0, sizeof(lck)) == NULL )
  { 
    fprintf( stderr,"System call memset() failed because of %s", strerror(errno));
   	exit(5);
  }
  // F_WRLCK mean exclusive lock and not shared lock
  /* difference, first put lock for read and write, in the second one
  process if another is reading from the file can read simultanealy 
  but cant write, and if 1 is writing no other one can write or read */
  lck.l_type = F_WRLCK;
  // lock entire file
  lck.l_whence = SEEK_SET; // offset base is start of the file "SEEK_END mean start at end of file"
  lck.l_start = 0;         // starting offset is zero
  lck.l_len = 0;           // len is zero, which is a special value representing end
                          // of file (no matter how large the file grows in future)

  /* OFD is a flag of Linux, not posix, more problably he become new standard in 
  POSIX 1. 
  The principal difference between OFD and non is that whereas
  traditional record locks are associated with a process, open file
  description locks(OFD) are associated with the open file description on
  which they are acquired, and are only automatically released on the last
  close of the open file description, instead of being released on any
  close of the file. 
  SETLKW mean is a blocked lock request*/
  
  //leva commento fcntl (fd, F_OFD_SETLKW, &lck); 
  
  // now we have the lock "load file in memory"

  /* initialize the memory for load the file, 
  fseek put the FP at END ftell say the position ( file size ), we come back at start with SEEK_SET*/
  FILE* fp = fdopen(fd, "r");
  if ( fp == NULL )
  { 
    fprintf( stderr,"System call fdopen() failed because of %s", strerror(errno));
   	exit(5);
  }
  fseek(fp, 0, SEEK_END);
  long len = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *file_content = malloc( (len+1) );
  fread( file_content, 1, len, fp);
  file_content[len]= '\0';

  // we check the tipe or file with file bash command
  /* char command[ (strlen(path))+5 ]; 
  command = strcat( "file", path );
  FILE* popen_output_stream = popen( command , "r" )
  if ( popen_output_stream == NULL )
  { 
    fprintf( stderr,"System call popen() failed because of %s", strerror(errno));
   	exit(5);
  }
  char* popen_output = malloc( Pat );
  while ( fgets(popen_output, 100, popen_output_stream) != NULL)
    printf("%s", path); */

  // release lock with F_UNLCK flag and FP FD
  fclose(fp);
  close(fd);
  lck.l_type = F_UNLCK;
  //leva commento  fcntl (fd, F_OFD_SETLKW, &lck);

  fprintf(stdout , "file: %s", file_content);
  free(file_content);
}

// this fuction open a listener socket
void open_socket()
{
  int on=1;
	struct sockaddr_in serv_addr;
  /*The socket() API returns a socket descriptor, which represents an endpoint.
    The statement also identifies that the INET (Internet Protocol) 
    address family with the TCP transport (SOCK_STREAM) is used for this socket.*/
  if ( (SERVER_SOCKET = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
  {
		fprintf( stderr,"System call socket() failed because of %s", strerror(errno));
   	exit(5);
	}

  /*The ioctl() API allows the local address to be reused when the server is restarted 
  before the required wait time expires. In this case, it sets the socket to be nonblocking. 
  All of the sockets for the incoming connections are also nonblocking because they inherit that state from the listening socket. */
  if ( (ioctl(SERVER_SOCKET, FIONBIO, (char *)&on)) < 0 )
  {
    fprintf( stderr,"System call ioctl() failed because of %s", strerror(errno));
    close(SERVER_SOCKET);
    exit(5);
  }

  /*declare sockaddr_in */   
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons( SERVER_PORT );

	// bind to join the unamed socket with sockaddr_in and become named socket
	if( bind( SERVER_SOCKET , (struct sockaddr*)&serv_addr ,  sizeof(serv_addr)) == -1 )
  {
		fprintf( stderr,"System call bind() failed because of %s", strerror(errno) );
		exit(5);
	}

  /* listen allows the server to accept incoming client connection  */
  if ( (listen( SERVER_SOCKET, 32)) < 0)
  {
    fprintf( stderr,"System call listen() failed because of %s", strerror(errno) );
		exit(5);
  }

}

// this function check if a line contain a new config
int check_if_conf(char line[])
{

  fprintf(stdout,"linea letta da conf:\n%s", line);
  int port_change=false;
  // if line is type "mode [t/p]"
  if( strncmp("mode",line,4)==0 )
  {
    char mode;
    memcpy( &mode, &line[5], 1 );
    if(mode=='t')
    {
      MODE_CLIENT_PROCESSING=0;
    }
    if(mode=='p')
    {
      MODE_CLIENT_PROCESSING=1;
    }
    //fprintf(stdout,"mode change %c: %d\n", mode, MODE_CLIENT_PROCESSING);
  }
  
  // if line is "port XXX" with XXX between (1000, 65500)
  if( strncmp("port",line,4)==0 )
  {
    long int val;
    val=strtol( &line[5], NULL, 10 );
    if( val != SERVER_PORT)
    {
      SERVER_PORT=val;
      //fprintf(stdout,"port change: %d\n", SERVER_PORT);
      port_change=true;
    }
  }
  return port_change;
}

// this function read the sacagawea.conf line by line 
int read_and_check_conf()
{
  // some declaretion 
  FILE *fp;
  const size_t max_line_size=100;
  char line[max_line_size];
  int end_while=true;
  int port_change=false;
  //open config file and check if an error occured
  fp = fopen("conf/sacagawea.conf", "r");
  if(fp==NULL)
  {
    fprintf( stderr,"System call fopen() failed because of %s", strerror(errno));
   	exit(5);
  }

  //readline or 100 char
  do{
    if( fgets( line, max_line_size, fp)==NULL)
    {
      if(feof(fp))
      {
        end_while=false;
      }else{
        fprintf( stderr,"System call fgets() failed because of %s", strerror(errno));
        exit(5);
      }
    }
    // check if the line is a config line
    if( (strlen(line)!=100) && (check_if_conf(line)) )
    {
      port_change=true;
    }
  }while(end_while);

  return port_change;
}

// this function is called when SIGHUP coming 
void config_handler(int signum)
{
  /* Check sagacawea.conf, if the return's value is true the socket SERVER_PORT 
  change so we have to close the socket finish the instaured connection
  and restart the socket with the new SERVER_PORT */
  if( read_and_check_conf() )
  {
    fprintf(stdout,"SERVER_SOCKET CHANGE %d\n",SERVER_SOCKET);
    /* shutdown with SHUT_WR stop the socket response, he don't send data anymore on that socket.
    so if a new connection request ( SYN ) coming he don't answert ( SYN ACK ). */
    if ( shutdown(SERVER_SOCKET, SHUT_WR) < 0 )
    {
      fprintf( stderr,"System call shutdown() failed because of %s\n", strerror(errno) );
      exit(5);
    }
    // now we accept all remaining connected comunication which did 3WHS
    int new_s;
    do
    {
      new_s = accept(SERVER_SOCKET, NULL, NULL);
      fprintf( stdout ,"new_s = %d\n", new_s );
      if (new_s < 0)
      {
        /* remember, we do a NON BLOCK socket, so if we have finished the waiting connections,
        accept will return -1 with EWOULDBLOCK errno */
        if (errno != EWOULDBLOCK)
        {
          fprintf( stderr,"System call accept() failed because of %s\n", strerror(errno) );
          exit(5);
        }else{
          break;
        }
      }else if(new_s > 0){
        /* add the descriptor associated at new connection at fds_set, then select can 
        controll when is readable */
        FD_SET(new_s, &fds_set);
      }
    } while ( new_s != 0);

    // close definitely the listen server socket
    close(SERVER_SOCKET);
    // Leave the closed socket from fds_set 
    FD_CLR( SERVER_SOCKET, &fds_set);
    if( SERVER_SOCKET==max_num_s)
    {
      while ( FD_ISSET(max_num_s , &fds_set) == false )
      {
        max_num_s--;
      }
    }
    // Open the new listen socket at new PORT
    open_socket();
    // Add new socket at set of socket to select
    FD_SET(SERVER_SOCKET, &fds_set);
    // in case, set the new max descriptor 
    if (SERVER_SOCKET > max_num_s)
    {  
      max_num_s = SERVER_SOCKET;
    }

  }
}

// this function call the select() and check the FDS_SET if some socket is readable
int listen_descriptor()
{
  // Some declaretion of usefull variable
  int i, num_fd_ready, check;
  struct timeval timeout;
  fd_set working_set;
  int close_conn;
  char input[80]; // momentanio per vedere se arriva l'input
  int new_s;
    
  /* Initialize the timeval struct to 13 minutes.  If no        
  activity after 13 minutes this program will end.           */
  timeout.tv_sec  = 13 * 60;
  timeout.tv_usec = 0;

  /* create a copy of fds_set called working_set, is a FD_SET to work on  */
  memcpy(&working_set, &fds_set, sizeof(fds_set));

  // start select and check if failed
  printf("Waiting on select()...\n");
  check = select( max_num_s+1, &working_set, NULL, NULL, &timeout);
  /* if errno==EINTR we have an system call error becouse sigaction 
  so we have to repeat select */
  if ( (check < 0) && (errno != EINTR) )
  {
    fprintf( stderr,"System call select() failed because of %s", strerror(errno) );
    exit(5);
  }// Chek if select timed out
  if (check == 0)
  {
    printf("select() timed out. End program.\n");
    return true;
  }
  /* 1 or more descriptors are readable we have to check which they are */
  num_fd_ready=check;
  // -----------------qui potrei far diventare funzione---------------
  // for, for check all ready FD in fds_set until, FD are finish or we check all the ready fd
  for (i=0;  i <= max_num_s && num_fd_ready > 0; ++i)
  {
    close_conn = false;
    // Check to see if the i-esimo descriptor is ready
    if (FD_ISSET(i, &working_set))
    {
      /* if we come there, the descriptor is readable. */
      num_fd_ready -= 1;

      if (i==SERVER_SOCKET)
      {
        printf("Listening socket is readable \n");
        /*Accept all incoming connections that are queued up on the listening socket before we
        loop back and call select again. */
        do
        {
          /*Accept each incoming connection.  If accept fails with EWOULDBLOCK,
          then we have accepted all of them.
          Any other failure on accept will cause us to end the server.  */
          new_s = accept(SERVER_SOCKET, NULL, NULL);
          if (new_s < 0)
          {
            if (errno != EWOULDBLOCK)
            {
              fprintf( stderr,"System call socket accept() failed because of %s", strerror(errno) );
              exit(5);
            }
            break;
          }
          /* we save all incoming connection, so we can check if a input come with the system call select */
          printf("New connection stabilished at fd - %d\n", new_s);
          FD_SET(new_s, &fds_set);
          /* check if the socket of the new accepted connection is greater then max_num_s, otherwise
          the biggest socket open */
          if (new_s > max_num_s)
            max_num_s = new_s;

        }while (new_s != -1);
          
      }else{ //This is not the listening socket, therefore an existing connection must be readable

        /* oppure qui possiamo creare il thread/processo che gestisce la connessione,
        poiche a questo punto abbiamo un socket di una connessione leggibile. lo passiamo al "figlio"
        e lo gestisce. a me piace piu cosi perche cosi ha senso l'uso della select, altrimenti 
        è come fare listen e accept bloccanti */
        /* if ( MODE_CLIENT_PROCESSING == 0)
        {
          thread_gesture();
        }else{
          process_gesture();
        }*/
        
        //provo per vedere se funziona mono thread "non eliminare"
        printf("  Descriptor %d is readable\n", i);
          
        /* Receive data on this connection until the recv fails with EWOULDBLOCK.
        If any other failure occurs, we will close the connection.    */
        check = recv(i, input, sizeof(input), 0);
        if (check < 0)
        {
          if (errno != EWOULDBLOCK)
          {
            // if recv fail the error can be server side or client side so we close the connection and go on 
            fprintf( stderr,"System call recv() of sd - %d, failed because of %s we close that connection", i, strerror(errno) );
            close_conn = true;
          }
          fprintf( stdout,"System call recv() of sd - %d EWOULDBLOCK", i );
          break;
        }
          /* Check to see if the connection has been closed by the client, so recv return 0  */
          if (check == 0)
          {
            printf("  Connection closed %d\n",i);
            close_conn = true;
            //---break;
          }
          // if we are there check is the number of bytes read from client, print that message
          printf("  %d bytes received\n", check);
          // stampo indietro il messaggio sempre prova per vedere il funzionamento "non eliminare"
          check = send(i, input, check, 0);
          if (check < 0)
          {
            // same of recv
            fprintf( stderr,"System call recv() of sd - %d, failed because of %s", i, strerror(errno) );
            //exit(5);
            // or can be a client error so we have only to close connection
            close_conn = true;
          }
          // fai la parte del rimuovi connessioni chiuse,aggiungi i check saltati prima

          /* we check if we have to close the connection of the sd and if is the max_num_s
          we have to determinate the new max */
          if(close_conn)
          {
            close(i);
            FD_CLR( i, &fds_set);
            if( i==max_num_s)
            {
              while ( FD_ISSET(max_num_s , &fds_set) ==false )
              {
                max_num_s--;
              }
            }

          }
        } // End else
      }
  } // End of select loop
  return false; 
}

int main(int argc, char *argv[])
{

  load_file_memory_posix( "conf/sacagawea.conf");
  // check the sacagawea.conf
  read_and_check_conf();
  // check if some variable are setted by command line
  int c;
  opterr=0;
	while ( ( c = getopt(argc, argv, "ptP:") ) != -1 ) {
		switch (c){
			case 'p':
				MODE_CLIENT_PROCESSING=1;
        fprintf(stdout,"mode change p: %d\n", MODE_CLIENT_PROCESSING);
	      break;

	    case 'P':
				SERVER_PORT = atoi(optarg);
        fprintf(stdout,"port change: %d\n", SERVER_PORT);
	    	break;

	    case 't':
			  MODE_CLIENT_PROCESSING=0;
        fprintf(stdout,"mode change t: %d\n", MODE_CLIENT_PROCESSING);
	    	break;

			case '?':
        fprintf( stdout,"Usage: sacagawea [-P number_of_port][-p/-t for use subprocess/threads to process 1 client connection]" );
				exit(20);
				break;
		}
	}

  fprintf( stdout, "Server port: %d, mode: %d\n", SERVER_PORT , MODE_CLIENT_PROCESSING);

  // Creating sigaction for SIGHUP
  struct sigaction new_action;
  /* Block other SIGHUP signals while handler runs. */
  sigset_t block_mask;
  sigemptyset (&block_mask);
  sigaddset (&block_mask, SIGHUP);
  /* Set up the structure to specify the new action. */
  new_action.sa_handler = config_handler;
  new_action.sa_mask = block_mask;
  sigemptyset (&new_action.sa_mask);
  /* set SA_RESTART flag, so If a signal handler is invoked meanwhile a system call 
  is running like read/recv etc.. after the handler,
  the system call is restarted and can give EINTR error if fail */ 
  new_action.sa_flags = SA_RESTART;
  /* The sigaction() API change the action taken by a process on receipt of SIGHUP signal. */
  if( sigaction (SIGHUP, &new_action, NULL) < 0 )
  {
    fprintf( stderr,"System call sigaction() failed because of %s", strerror(errno));
   	exit(5);
  }
    
  // open socket call
  open_socket();

  int i;
  
  /* declare FD_SET and initialize it */
  FD_ZERO(&fds_set);
  max_num_s = SERVER_SOCKET;
  FD_SET( SERVER_SOCKET, &fds_set);
  //per controlare roba da me "non eliminare"
  for (i=0;  i <= max_num_s ; ++i)
  {
    fprintf( stdout,"i: %d  is set:  %d\n",i,FD_ISSET(i, &fds_set));
  }

  /* Loop waiting for incoming connects or for incoming data
    on any of the connected sockets.   */
  do
  {
    if( listen_descriptor() ){
      break;
    }
  }while(true);

  // we are out of select loop so we have to close all connection
  for (i=0; i <= max_num_s; ++i)
  {
    if (FD_ISSET(i, &fds_set))
    {
      close(i);
    }
  }

}
