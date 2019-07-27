# `Linee di dipendenza`

### contengono un ":"
* alla sinistra ci sono le dipendenze
* alla destra ci sono i sorgenti necessari per costruire la dipendenza

```makefile
hello.o : hi.c hello.c ciao.c
```

Quando si lancia il comando	make, vengono controllate la data e l’ora in cui il target (le dipendenze) è stato creato e confrontate con quelle dei sorgenti necessari per costruirle. Se uno qualsiasi ha una data	più	recente, allora	viene eseguita la linea comandi dopo la linea di dipendenza.

```è importante che tutte le dipendenze siano piazzate in ordine discendente nel makefile```

# `Linee di comandi (shell)`

devono avere un tab iniziale e seguono una linea di dipendenza

```make
hello.o : hi.c hello.c ciao.c
    gcc hi.c hello.c ciao.c 
```

make controlla se ci sono errori in questi comandi shell, se ci sono errori ritorna un valore != 0 e make termina l'esecuzione. 

Gli errori possono essere ignorati con l'opzione -i di make

# `Macro`

Associa qualcosa come una stringa, codice ecc ad un alias.

Per recuperare il qualcosa dal alias si usa $()


Esempi di macro:
```m
HOME = /home/corso/pds/spring17
CPP = $(HOME)/cpp
TCPP = $(HOME)/tcpp
PROJ = .
INCL = -I $(PROJ) –I $(CPP) –I $(TCPP)
```

Si possono definire macro anche sulla linea comandi di make.
Queste macro hanno la precedenza su quelle definite nel makefile. es:
```m
Make HOME = /home/esame/aaa/finale
```











