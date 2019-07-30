CC = gcc
WCC = x86_64-w64-mingw32-gcc

SRC      = ./src
BUILD    = ./build
BIN      = ./bin

CFLAGS  = -Wall -O3

#included by both Linux and windows
INC  = -I$(SRC)/include

HEADERS = $(wildcard ${SRC}/include/*.h)

L_HEADERS = $(wildcard ${SRC}/include/linux/*.h)
L_SOURCES = $(wildcard ${SRC}/linux/*.c)
L_OBJS    = $(patsubst ${SRC}%.c, ${BUILD}%.o, $(L_SOURCES))

W_HEADERS = $(wildcard ${SRC}/include/win32/*.h)
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
	$(CC) $(CFLAGS) $(INC) -shared -o ${BIN}/libsacagawea.so ${L_OBJS}

$(L_OBJS): $(L_SOURCES) $(HEADERS) $(L_HEADERS)
	$(CC) $(CFLAGS) $(INC) -c -fpic $< -o $@


server: $(SVR_OBJS)

$(SVR_OBJS): $(SVR_SOURCES)
	@echo 
	@echo "#####################"
	@echo "Building Linux Server"
	@echo "#####################"
	
	$(CC) $(CFLAGS) $(INC) -L${BIN} -lsacagawea -lpthread -Wl,-rpath=. -o ${BIN}/sacagawea.out $(SVR_SOURCES)
	
	@echo 
	@echo "#####################"
	@echo "Building Win32 Server"
	@echo "#####################"
	
	$(WCC) $(CFLAGS) $(INC) -L${BIN} -lsacagawea -Wl,-rpath=. -o ${BIN}/sacagawea.exe $(SVR_SOURCES)


win32: $(W_OBJS)
	$(WCC) $(CFLAGS) -shared -o ${BIN}/sacagawea.dll $(W_OBJS) -Wl,--out-implib,$(BUILD)/win32/sacagawea_dll.a

$(W_OBJS): $(W_SOURCES) $(HEADERS) $(W_HEADERS)
	$(WCC) $(CFLAGS) $(INC) -c -DBUILDING_SACAGALIB_DLL $< -o $@

.PHONY: clean

clean:
	rm -rf $(BIN)
	rm -rf $(BUILD)