CC = gcc
CC = mingw64-gcc

SRC      = ./src
BUILD    = ./build
BIN      = ./bin

CFLAGS  = -Wall -O3

#included by both Linux and windows
INC  = -I$(SRC)/headers

L_SOURCES = $(wildcard ${SRC}/linux/*.c)
L_OBJS    = $(patsubst ${SRC}%.c, ${BUILD}%.o, $(L_SOURCES))

W_SOURCES = $(wildcard ${SRC}/win32/*.c)
W_OBJS    = $(patsubst ${SRC}%.c, ${BUILD}%.o, $(W_SOURCES))

SVR_SOURCES = $(wildcard ${SRC}/server/*.c)
SVR_OBJS    = $(patsubst ${SRC}%.c, ${BUILD}%.o, $(SVR_SOURCES))


all: makedirs linux win32 server
# copy default sacagawea.conf file into bin directory
# so we always have the original
	cp sacagawea.conf bin

makedirs:
	@mkdir -p $(BIN)
	@mkdir -p ${BUILD}/linux
	@mkdir -p ${BUILD}/win32


linux: $(L_OBJS)
	$(CC) $(CFLAGS) $(LINC) $(INC) -shared -o ${BIN}/libsacagawea.so ${L_OBJS}

$(L_OBJS): $(L_SOURCES)
	$(CC) $(CFLAGS) $(LINC) $(INC) -c -fpic $< -o $@


server: $(SVR_OBJS)

$(SVR_OBJS): $(SVR_SOURCES)
	$(CC) $(CFLAGS) $(LINC) $(INC) -L${BIN} -Wl,-rpath=. -o ${BIN}/sacagawea.out $(SVR_SOURCES) -lsacagawea -lpthread


win32: 
	$(CC) $(CFLAGS) $(LINC) $(INC) -shared -o ${BIN}/libsacagawea.so ${L_OBJS}
