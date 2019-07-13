### Gopher 
è un protocollo TCP/IP di livello applicazione, nato in concomitanza con HTTP ma senza prendere piede.
Il suo funzionamento rimanda alla gerarchia di un file system. In effetti l'accesso ad un server gopher permette di montare 
il suo file system sul client.

---
---

### Caratteristiche
* Nato per essere utilizzato su computer di bassa potenza (1 MB Macs and DOS machines) è pertanto molto semplice
* E' stateless
* 

---
---

### Funzionamento
* Inizia con una richiesta di connessione da parte del client al server via TCP, contenente un SELECTOR ( ovvero una linea 
di testo che può anche essere vuota che indica "mandami la lista di quello che hai"). 
* Il server risponde con un blocco di testo terminato da una riga contenete un solo punto ".".
---
piccolo esempio \t sta per il TAB
```
 0About internet Gopher\t\tStuff:About us\trawBits.micro.umn.edu\t70
 1Around University of Minnesota\tZ,5692,AUMFunderdog.micro.umn.edu\t70
 1Departmental Publications\tStuff:DP:FrawBits.micro.umn.edu\t70
 .
```
* Il primo carattere di ogni riga descrive il tipo di file ( documento , directory etc... )
* La prima stringa fino al \t indica il nome del file per il client, ed in teoria l'unica cosa visibile
all'utente della risposta. Clicchera su di esso per inviere richieste future
* La seconda stringa è da utilizzare nella SELECTOR cosi che il server sappia di che file si parli 
( può essere un nome , un path qualsiasi cosa riconoscibile per il server)
* La terza stringa indica il domain-name dell'host che ha il file 
* l'ultimo campo dopo l'ultimo TAB indica la porta a cui connettersi
* CR LF sono carrieg return and line field.
```
Nell'esempio la linea 1 discrive un documento di nome "About internet Gopher" reperibile mandando 
la stringa "Struff:About us" a rawBits.micro.umn.edu porta 70
```
---














