# `Glossario`
* 1. BSD Unix: (Barkeley Software Distribution) è una variante originaria di Unix.
* 2. buffering: è il meccanisco di memorizzazione di dati in buffer ovvero contenitori di memoria contigui
* 3. user / kernel space:
la memoria è divisa in due aree distinte, 1 utente ovvero dove si trovano i programmi in esecuzione ecc...
2 kernel dove si trova il codice kernel
* 4. sincrono e asincrono:
sincrona è in tempo reale es chiamata telefonica
asincrono la risposta è in differita es un messaggio
* 5. 3WHS (three way hand shake del tcp/ip)p

## `Dominio` 
il dominio specifica dove è possibile comunicare, come sono specificati nomi e indirizzi.
### Il dominio UNIX (PF_UNIX)
permette la comunicazione solo con un processo sulla stessa macchina, molto simile ad una named pipe supporta sia Stream che Datagramm.
### Il dominio Internet (PF_INET)
permette la comunicazione tramite rete usando IP:port

# `UNIX SOCKET`
i socket sono un meccanismo di comunicazione tra processi (originario di BSD Unix *1 ma ormai supportato da tutti).
supportano varie famiglie di protocolli di comunicazione come TCP/IP, Novell IPX/SPX.
Forniscono il meccanismo di buffering^2 tra l' user space e il kernel space che sono rispettivamente sincrono e asincrono^3 / 4.
esistono 3 tipi di socket principalmente
* `1. Stream socket` sono affidabili con flusso bidirezionale di dati ordinati, non duplicati, non corrotti. 
`Unix` per crearlo si usa l'attributo SOCK_STREAM nella funzione socket(PF_INET, `SOCK_STREAM`, 0); Invece `Win32` usa il tipo (SOCKET)
* `2. Datagram socket` consegna pacchetti ma non affidabilmente `Unix` per crearlo usa l'attributo SOCK_DGRAM
* `3. Raw Socket` permette di utilizzare protocolli definiti dall'utente che si interfacciano direttamente con l'IP `Unix` usa l'attributo SOCK_RAW pre crearlo

a questo punto si ha una `Unamed socket` ovvero senza indirizzo, per dedicargli un indirizzo si una una struttura dati in base al dominio. Nel caso di internet si usano le strutture `in_addr` per indicare l'indirizzo IP `sockaddr_in` che contiene in_addr, la porta e la famiglia es AF_INET
```c
struct in_addr {
    unsigned long s_addr;
};

struct sockaddr_in{
    short sin_family;
    u_short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};
```
a questo punto abbiamo una `Named socket` che può essere messa in "ascolto" con la funzione `Unix` listen(). durante lo stato di listening il kernel mantiene 2 code. una per le connessioni incomplete ovvero che non hanno completato il 3WHS^5, una coda per le connessioni ESTABLISHED. l'argomento backlog della listen specifica il valore massimo di queste code. Se piene e arriva un SYN semplicemente viene ignorato. `NB` I dati che arrivano tra il completamento del 3WHS e l'accept() sono messi fino a riempire il buffer di ricezione del socket.
Per altri dettagli vedi TCP/IP di reti.

---

# `I/O Models`
```c
ssize_t send(int s, const void *msg, size_t len, int flags);

ssize_t recv(int s, void *buf, size_t len, int flags);
```
L'argomento flags indica vari elementi `MSG DONTWAIT` rende l'operazione Non bloccante e ritorna EAGAIN nel caso l'operazione avrebbe bloccato `MSG NOSIGNAL` richiede di non inviare SIGPIPE se l'estremo chiude il socket `nb` ce ne sono altri e non tutte le piattaforme supportano tutti questi flags

```c
ssize_t writev(int d, const struct iovec *iov, int iovcnt);

ssize_t readv(int d, const struct iovec *iov, int iovcnt);

struct iovec {char *iov_base; /* Base address. */
              size_t iov_len; /* Length. */ };
```

sono primitive che permettono operazioni di I/O non contigue. iovec specifica l’indirizzo base e la lunghezza di ogni blocco.

### `I/O Multiplexing con SELECT`

La select controlla in maniera sincrona un insieme di descrittori, sotto il controllo di un timer
```c
int select (int maxfdp1, fd_set *readfds, fd_set *writefds,
fd_set *exceptfds, struct timeval *timeout);
```

* `maxfdp1`: max num descrittori da controllare (+ 1);
* `readfds`: insieme di descrittori da controllare per la lettura
e la richiesta di connessione;
* `writefds`: insieme di descrittori da controllare per la scrittura
ed il completamento di connessioni in uscita;
* `exceptfds`: insieme di descrittori da controllare per
‘‘urgent data’’;
* `timeout`: timeout per l’attesa.

```c
#include <sys/select.h>
questa libreria gestisce gli FD_SET

void FD_ZERO(fd_set *fdset);

int FD_ISSET(int fd, fd_set *fdset); /* return 1 if fd is set */
/* 0 otherwise */

void FD_CLR(int fd, fd_set *fdset);

void FD_SET(int fd, fd_set *fdset);
```
select ritorna il numero di descrittori pronti, oppure 0 se scade il
timeout. Un socket è pronto in lettura se è vera una delle seguenti condizioni
* il numero di byte nel buffer di ricezione è ≥ low
water mark (tipicamente uguale ad 1 byte);
* la connessione TCP ha ricevuto un FIN (read torna 0)
* il socket è nello stato listening ed il numero di connessioni
completate è ≥ 1
* il socket è in errore. (read non blocca e torna un errore)

un socket è pronto in scrittura se è vera una delle seguenti condizioni
* Il numero di byte disponibili nel buffer di invio del socket è ≥ del low water mark (tipicamente uguale ad 1 byte) ed il socket e
connesso oppure non richiede una connessione (SOCK_DGRAM).
* il socket aveva in corso una connect non bloccante che è
completata (oppure ha fallito);
* La connessione `e stata chiusa. Una write sul socket genera un
SIGPIPE.
* Il socket `e in errore.


---

# `Gestione delle opzioni`

setsockopt: definisce un’opzione sul socket
```c
int setsockopt (int s, int level, int optname,void *optval, int optlen);
```
getsockopt: ritorna il valore di un’opzione del socket
```c
int getsockopt (int s, int level, int optname,void *optval, int *optlenptr);
```

* `level`: livello del protocollo
(SOL_SOCKET, IPPROTO_TCP, IPPROTO_IP,
IPPROTO_ICMPV6, IPPROTO_IPV6)
* `optname`: vedi successive slide...
* `optval`: valore dell’opzione
* `optlen`: lunghezza dell’opzione

### `Principali opzioni`
* `SO_KEEPALIVE`: Abilita l’invio di pacchetti di ‘‘keep-alive’’.
* `SO_OOBINLINE`: I dati out-of-band sono direttamente inseriti nel flusso dei dati in ricezione
* `SO_RCVLOWAT e SO_SNDLOWAT`: Specificano il numero minimo di byte nei buffer di ricezione prima che i dati vengano passati all’applicazione (SO_RCVLOWAT) o al protocollo (SO_SNDLOWAT)
* `SO_RCVTIMEO e SO_SNDTIMEO`, `SO_KEEPALIVE`, `SO_REUSEADDR`, `SO_TYPE`, `SO_BROADCAST`, `SO_SNDBUF`, `SO_RCVBUF`, `SO_LINGER` e tanti altri.














