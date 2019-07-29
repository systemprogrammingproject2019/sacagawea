# `START`

* use command `make` on the right folder and after run the exe
with -P "port number" -p or -t for thread/process management.
```bash
cd ..../progetto
make
./sacagawea.out -P 5467
```
* now on the terminal u can see some INFO line, the first is the information read on sacagawea.conf, line by line.
```
linea letta da conf:
`linea letta`
```

* now u can see if mode or / port is changed from command line
```
port change: XXX
```

* at that moment he create the socket and print the information or the server 
```
Server port: 5467, mode: 0
```
* ignore the next line `i:0 is set:0` 
---
---
# `CLIENT CONN`
* open new terminal and do this command for connect
### curl -v telnet://IP:PORT 
es:
```bash
curl -v telnet://localhost:5467
```
* now u can see on the terminal where server is running the incoming of 1 connection
```
New connection stabilished at fd - 5 from ��nY�
SOCKET: 5
IP:PORT: 127.0.0.1:4328
Waiting on select()...
```
thats mean which the connection is stabilished and the server return to waiting on select.

* from client terminal now u can send the SELECTOR. Remember the selector end with \n so write a line and send it with "ENTER".
the server responds with the same message sent, da implementare il continuo eheheh.
* after the \n all bytes/char u send will be ignored
---
---
# `CHECK THE PORT SWITCH`
* u need the pid of server, u can take that by 2 ways. 
1. first u can run background the server and see the [ pid ] , with 
```
./sacagawea.out -P 5467 &
```
2. use the command below and see the pid of server
```
netstat -puttana | grep 5467
```
* at this moment send the SIGHUP
```
kill -1 PID
```
* now curl at new port
---




