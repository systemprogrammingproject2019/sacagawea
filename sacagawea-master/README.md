# Sacagawea

Sacagawea is a gopher server.

---

# `libsacagawea.so` / `sacagawea32.dll`

* exclusive lock to access to files (even in read-only mode)
* determine file type by calling `file` with popen (on Linux) or by checking the extension (on Windows)
* map file in memory (will need to do this before sending it, in the server)

---

# `sacagawea(.exe)`

### These will be coded inside the library, and called by sacagawea(.exe)
* Check the configuration file (sacagawea.conf), then parse command line options
* create sighandler for SIGHUP (on Linux) or listener for console event (on Windows), this will check the configuration file
* spawn sacalogs(.exe) and make a new pipe between the main process and sacalogs

---

### These will be coded inside sacagawea(.exe)

* bind/listen/select/accept
* spawn new process/thread which will carry on the comunication
* the new process/thread loads the file to send in memory
* send and event/condition variable to sacalogs(.exe)
* always spawn a new thread to send the loaded file to the client

---

# `sacalogs(.exe)`

* logFile process which receives information on the operation performed by a pipe (normal pipe)
* it wake up with an event (on Win32), or a condition variable (on Linux)

The saved logs (sacagawea.log) will use the following syntax:

```pseudocode
[dd-MM-YYYY hh:mm:ss] file_name, file_size, client's IP:port
```

For example:
```pseudocode
[21-07-2019 15:54:33] mare.jpg, 2048 B, 72.192.10.4:3948
```

---

# `sacagawea.conf`
Regex to find all configuration variables
```perl
$regex =~ /([a-zA-Z0-9]+)[ \t]+([a-zA-Z0-9 ]+)(:?\n|$)/;
```
---