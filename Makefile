CC = gcc
WCC = x86_64-w64-mingw32-gcc

SRC      = ./src
INCLUDE  = $(SRC)/include
BUILD    = ./build
BIN      = ./bin

CFLAGS  = -Wall -O3 -g 

#included by both Linux and windows
INC  = -I$(INCLUDE)

HEADERS = $(wildcard ${INCLUDE}/*.h)

L_HEADERS = $(wildcard ${INCLUDE}/linux/*.h)
L_SOURCES = $(wildcard ${SRC}/linux/*.c)
L_OBJS    = $(patsubst ${SRC}%.c, ${BUILD}%.o, $(L_SOURCES))

W_HEADERS = $(wildcard ${INCLUDE}/win32/*.h)
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

	$(CC) $(CFLAGS) $(INC) -L${BIN} -Wl,-rpath=. -o ${BIN}/sacagawea.out $(SVR_SOURCES) -lsacagawea -lpthread -lrt

	@echo 
	@echo "#####################"
	@echo "Building Win32 Server"
	@echo "#####################"

	$(WCC) $(CFLAGS) $(INC) -L${BIN} -Wl,-rpath=. -o ${BIN}/sacagawea.exe $(SVR_SOURCES) -lws2_32 -lsacagawea

win32: $(W_OBJS)
	$(WCC) $(CFLAGS) -shared -o ${BIN}/sacagawea.dll $(W_OBJS) -Wl,--out-implib,$(BUILD)/win32/sacagawea_dll.a -lws2_32

$(W_OBJS): $(W_SOURCES) $(HEADERS) $(W_HEADERS)
	$(WCC) $(CFLAGS) $(INC) -c -DBUILDING_SACAGALIB_DLL $< -o $@ -lws2_32

.PHONY: clean lrun wrun

lrun: linux server
	cd bin && ./sacagawea.out

wrun: win32 server
	cd bin && wine ./sacagawea.exe

clean:
	rm -rf $(BIN)
	rm -rf $(BUILD)
