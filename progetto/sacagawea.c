#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>

#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>



#define true     1
#define false    0

/*
if a system call failed the program exit with status 5
*/

void openConnection(int IP, int PORT){

}


int main(int argc, char *argv[])
{
  // check if some variable are setted by command line
  //fprintf( stdout,"Usage: sacagawea [-P number_of_port][-p/-t for use subprocess/threads to process 1 client connection]" );

  int i, num_fd_ready, check;
  int    close_conn;
  char input[80]; // momentanio per vedere se arriva l'input
  int on=1;
  int internal_s, max_num_s, new_s;
  struct timeval timeout;
	struct sockaddr_in serv_addr;
  fd_set fds_set, working_set;
  
  /*The socket() API returns a socket descriptor, which represents an endpoint.
    The statement also identifies that the INET (Internet Protocol) 
    address family with the TCP transport (SOCK_STREAM) is used for this socket.*/
  if ( (internal_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
  {
		fprintf( stderr,"System call socket socket() failed because of %s", strerror(errno));
   	exit(5);
	}

  /*The ioctl() API allows the local address to be reused when the server is restarted 
  before the required wait time expires. In this case, it sets the socket to be nonblocking. 
  All of the sockets for the incoming connections are also nonblocking because they inherit that state from the listening socket. */
  if ( (ioctl(internal_s, FIONBIO, (char *)&on)) < 0 )
  {
    fprintf( stderr,"System call socket ioctl() failed because of %s", strerror(errno));
    close(internal_s);
    exit(5);
  }

  /*declare sockaddr_in */   
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons( atoi( "7070" ) );

	// bind to join the unamed socket with sockaddr_in and become named socket
	if( bind( internal_s , (struct sockaddr*)&serv_addr ,  sizeof(serv_addr)) == -1 )
  {
		fprintf( stderr,"System call socket bind() failed because of %s", strerror(errno) );
		exit(5);
	}

  /* listen allows the server to accept incoming client connection  */
  if ( (listen( internal_s, 32)) < 0)
  {
    fprintf( stderr,"System call socket listen() failed because of %s", strerror(errno) );
		exit(5);
  }

  /* declare FD_SET and initialize it */
  FD_ZERO(&fds_set);
  max_num_s = internal_s;
  FD_SET( internal_s, &fds_set);
  //per controlare roba da me "non eliminare"
  for (i=0;  i <= max_num_s && num_fd_ready > 0; ++i)
  {
    printf("i: %d  is set:  %d\n",i,FD_ISSET(i, &fds_set));
  }

  /* Initialize the timeval struct to 3 minutes.  If no        
  activity after 3 minutes this program will end.           */
  timeout.tv_sec  = 3 * 60;
  timeout.tv_usec = 0;

  /* Loop waiting for incoming connects or for incoming data
    on any of the connected sockets.   */
  do
  {
    /* create a copy of fds_set called working_set, or the fd_set to work on     */
    memcpy(&working_set, &fds_set, sizeof(fds_set));

    // start select and check if failed
    printf("Waiting on select()...\n");
    check = select( max_num_s+1, &working_set, NULL, NULL, &timeout);
    if (check < 0)
    {
      fprintf( stderr,"System call socket select() failed because of %s", strerror(errno) );
      exit(5);
    }// Chek if select timed out
    if (check == 0)
    {
      printf("  select() timed out.  End program.\n");
      break;
    }
    /* 1 or more descriptors are readable we have to check which they are */
    num_fd_ready=check;

    // for, for check all ready FD in fds_set until, FD are finish or we check all the ready fd
    for (i=0;  i <= max_num_s && num_fd_ready > 0; ++i)
    {
      //Check to see if the i-esimo descriptor is ready
      if (FD_ISSET(i, &working_set))
      {
        /* if we come there, the descriptor is readable. */
        num_fd_ready -= 1;

        if (i==internal_s)
        {
          printf("Listening socket is readable \n");
          /*Accept all incoming connections that are queued up on the listening socket before we
          loop back and call select again. */
          do
          {
            /*Accept each incoming connection.  If accept fails with EWOULDBLOCK,
            then we have accepted all of them.
            Any other failure on accept will cause us to end the server.  */
            new_s = accept(internal_s, NULL, NULL);
            if (new_s < 0)
            {
              if (errno != EWOULDBLOCK)
              {
                fprintf( stderr,"System call socket accept() failed because of %s", strerror(errno) );
                exit(5);
              }
              break;
            }
            /* qui possiamo fare la creazione del thread/processo che gestisce la connessione */
            // intanto mi salvo tutte le connessioni socket create dentro fds_set, cosi select le controlla (mono thread)
            printf("New connection stabilished at fd - %d\n", new_s);
            FD_SET(new_s, &fds_set);
            // se il nuovo fd dato alla nuova connessione è maggiore del massimo gia salvato allora diventa 
            // il nuovo massimo 
            if (new_s > max_num_s)
              max_num_s = new_s;

          }while (new_s != -1);
          
        }else{ //This is not the listening socket, therefore an existing connection must be readable

          /* oppure qui possiamo creare il thread/processo che gestisce la connessione,
          poiche a questo punto abbiamo un socket di una connessione leggibile. lo passiamo al "figlio"
          e lo gestisce. a me piace piu cosi perche cosi ha senso l'uso della select, altrimenti 
          è come fare listen e accept bloccanti */

          //provo per vedere se funziona mono thread "non eliminare"
          printf("  Descriptor %d is readable\n", i);
          do
          {
            /* Receive data on this connection until the recv fails with EWOULDBLOCK.
            If any other failure occurs, we will close the connection.    */
            check = recv(i, input, sizeof(input), 0);
            if (check < 0)
            {
              if (errno != EWOULDBLOCK)
              {
                // if recv fail the error can be the server so we have to close server 
                fprintf( stderr,"System call recv() of sd - %d, failed because of %s", i, strerror(errno) );
                //exit(5);
                // or can be a client error so we have only to close connection
                close_conn = true;
              }
              break;
            }
            /* Check to see if the connection has been closed by the client, so recv return 0  */
            if (check == 0)
            {
              printf("  Connection closed\n");
              close_conn = true;
              break;
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
            } while (true);

            /* we check if we have to close the connection of the sd and if is the max_num_s
            we have to determinate the new max */
            if(close_conn)
            {
              close(i);
              FD_CLR( i, &fds_set);
              if(i==max_num_s)
              {
                while ( FD_ISSET(max_num_s , &fds_set) ==false )
                {
                  max_num_s -= 1;
                }
              }

            }

        } // End else
      } 
    } // End of select loop
  }while(true);

  // we are out of selext loop so we have to close all connection
  for (i=0; i <= max_num_s; ++i)
  {
    if (FD_ISSET(i, &fds_set))
    {
      close(i);
    }
  }

}