sa : sacagaweaLinux.c sacagawea.c sacagawea.h
	gcc -c -fpic sacagaweaLinux.c
	gcc -shared -o libsacagawea.so sacagaweaLinux.o
	gcc -L/home/michele/Desktop/progetto -Wl,-rpath=/home/michele/Desktop/progetto -o sacagawea.out sacagawea.c -lsacagawea -lpthread
	
