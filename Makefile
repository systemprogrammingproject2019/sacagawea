CC = gcc

SRC      = ./src
BUILD    = ./build
BIN      = ./bin

CFLAGS  = -Wall -O3
LINC = -I$(SRC)/linux/headers
WINC = -I$(SRC)/win32/headers

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
	$(CC) $(CFLAGS) $(LINC) -shared -o ${BIN}/libsacagawea.so ${L_OBJS}

$(L_OBJS): $(L_SOURCES)
	$(CC) $(CFLAGS) $(LINC) -c -fpic $< -o $@


server: $(SVR_OBJS)

$(SVR_OBJS): $(SVR_SOURCES)
	$(CC) $(CFLAGS) $(LINC) -L${BIN} -Wl,-rpath=. -o ${BIN}/sacagawea.out $(SVR_SOURCES) -lsacagawea -lpthread


win32: 
