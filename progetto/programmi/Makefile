SACAPATH=$$(pwd)
	
sa : sacagaweaLinux.c sacagawea.c sacagawea.h
	gcc -c -fpic sacagaweaLinux.c
	gcc -shared -o libsacagawea.so sacagaweaLinux.o
	gcc -L${SACAPATH} -Wl,-rpath=${SACAPATH} -o sacagawea.out sacagawea.c -lsacagawea -lpthread
