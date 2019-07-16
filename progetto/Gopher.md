# `Gopher `
è un protocollo TCP/IP di livello applicazione, nato in concomitanza con HTTP ma senza prendere piede.
Il suo funzionamento rimanda alla gerarchia di un file system. In effetti l'accesso ad un server gopher permette di montare 
il suo file system sul client.

---
---


# `Caratteristiche server Gopher`
* Nato per essere utilizzato su computer di bassa potenza (1 MB Macs and DOS machines) è pertanto molto semplice
* E' stateless
* Riceve solamente path, i quali indicano verso file o directory (eliminando eventuali CR-LF o TAB)

# `Locazione servizi`
* I documenti sono linkati ad una macchina dalla tripla (selector-string, dominio, IP:porta)
* Ci dovrà essere una top-level o root server
* le informazioni in un server possono essere duplicate in piu server,questi server secondari necessitano di una registrazione ovvero fornire la loro coppia (dominio, IP:porta) al server gopher top-level
* E' bene che ogni server registrato utilizzi il nome di dominio CNAME ovvero l'alias per farsi localizzare da un client
* Di base si usa la porta 70
* E' bene che ogni server amministratore abbia un documento chiamato tipo "about me" che contiene alcune informazioni e descrizioni circa il server
* Il server deve essere in grado di redirezionare la ricerca ad un server Gopher di ricerca, che dati una stringa di ricerca o un insieme di strighe separate da spazi visti come AND dal server es
``` 
ciao paolo cc
```

# `tipi file`
* l'insieme di caratteri indicatori di tipo iniziali di base sono {0,1,7} che indicano rispettivamente {file, directory, ricerca}
* ulteriori servizi vanno specificati da una lettera, cosi che se il client riceve per esempio il carattere '2' sa che deve saper leggere il procollo CSO


```
0   The item is a TextFile Entity. Client should use a TextFile Transaction.

1   The item is a Menu Entity. Client should use a Menu Transaction.

2   The information applies to a CSO phone book entity. Client should talk CSO protocol.

3   Signals an error condition.

4   Item is a Macintosh file encoded in BINHEX format

5   Item is PC-DOS binary file of some sort.  Client gets to decide.

6   Item is a uuencoded file.

7   The information applies to a Index Server. Client should use a FullText Search transaction.

8   The information applies to a Telnet session. Connect to given host at given port. The name to login as at this host is in the selector string.

9   Item is a binary file.  Client must decide what to do with it.

+   The information applies to a duplicated server.  The information contained within is a duplicate of the primary server.  The primary server is defined as the last DirEntity that is has a non-plus "Type" field.  The client should use the transaction as defined by the primary server Type field.

g   Item is a GIF graphic file.

I   Item is some kind of image file.  Client gets to decide.

T   The information applies to a tn3270 based telnet session.
Connect to given host at given port. The name to login as at this host is in the selector string.`
```

---
---

# `Funzionamento`
* Inizia con una richiesta di connessione da parte del client al server via TCP, contenente un SELECTOR ( ovvero una linea 
di testo che può anche essere vuota che indica "mandami la lista di quello che hai").
* Il server risponde con un blocco di testo terminato da una riga contenete un solo punto ".".
---
### `piccolo esempio (\t sta per il TAB)`
```
 0About internet Gopher\t\tStuff:About us\trawBits.micro.umn.edu\t70
 1Around University of Minnesota\tZ,5692,AUM\tunderdog.micro.umn.edu\t70
 1Departmental Publications\tStuff:DP:\trawBits.micro.umn.edu\t70
 .
```
* Il primo carattere di ogni riga descrive il tipo di file ( documento , directory etc... )
* La prima stringa fino al \t indica il nome del file per il client, ed in teoria l'unica cosa visibile
all'utente della risposta. Clicchera su di esso per inviere richieste future
* La seconda stringa è da utilizzare nella SELECTOR cosi che il server sappia di che file si parli
( può essere un nome , un path qualsiasi cosa riconoscibile per il server)
* La terza stringa indica il domain-name dell'host che ha il file
* l'ultimo campo dopo l'ultimo TAB indica la porta a cui connettersi
* CR LF sono carrieg return and line feed.
```
Nell'esempio precedente vediamo la linea 1 descrivere un documento di nome "About internet Gopher" reperibile mandando la stringa "Struff:About us" a rawBits.micro.umn.edu porta 70
```
---
