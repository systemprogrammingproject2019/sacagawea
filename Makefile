CC := gcc
WCC := x86_64-w64-mingw32-gcc

SRC         := ./src
LIB_SRC     := $(SRC)/sacagalib
INCLUDE     := $(SRC)/include
LIB_INCLUDE := $(SRC)/sacagalib/include
BUILD       := ./build
BUILD_SL    := $(BUILD)/linux
BUILD_WIN_SL:= $(BUILD)/win32
BIN         := ./bin

CFLAGS  := -Wall -O3 -g

#included by both Linux and windows
INC      := -I$(INCLUDE)
LIB_INC  := -I$(LIB_INCLUDE)

HEADERS     := $(wildcard ${INCLUDE}/*.h)
LIB_HEADERS := $(wildcard ${LIB_INCLUDE}/*.h)

LIB_SOURCES := $(wildcard ${LIB_SRC}/*.c)
LIB_HEADERS := $(wildcard ${LIB_INCLUDE}/*.h)
LIB_OBJS    := $(patsubst ${SRC}%.c, ${BUILD_SL}%.o, $(LIB_SOURCES))
LIB_WIN_OBJS:= $(patsubst ${SRC}%.c, ${BUILD_WIN_SL}%.o, $(LIB_SOURCES))

SACAGALIB_O := ${BUILD}/sacagalib/sacagalib.o

SVR_SOURCES := $(wildcard ${SRC}/server/*.c)
SVR_OBJS    := $(patsubst ${SRC}%.c, ${BUILD}%.o, $(SVR_SOURCES))


all: linux win32
# copy default sacagawea.conf file into bin directory
# so we always have the original

linux: makedirs linuxlib linuxserver
	@cp -u sacagawea.conf bin

win32: makedirs win32lib win32server
	@cp -u sacagawea.conf bin
	@cp -u lib/libpcre2-8-0.dll bin


makedirs:
	@mkdir -p $(BIN)
	@mkdir -p ${BUILD}/linux/sacagalib
	@mkdir -p ${BUILD}/win32/sacagalib

linuxlib: $(LIB_OBJS)
	$(CC) $(CFLAGS) $(LIB_INC) -shared -o ${BIN}/libsacagawea.so ${LIB_OBJS}

win32lib: $(LIB_WIN_OBJS)
	$(WCC) $(CFLAGS) -shared -o ${BIN}/sacagawea.dll $(LIB_WIN_OBJS) -Wl,--out-implib,$(BUILD)/win32/sacagawea_dll.a -lws2_32 -lpcre2-8 -lpcre2-posix -lshlwapi


$(BUILD_SL)/sacagalib/sacagalib.o: $(LIB_SRC)/sacagalib.c $(LIB_HEADERS)
	$(CC) $(CFLAGS) $(LIB_INC) -c -fpic $< -o $@

$(BUILD_SL)/sacagalib/socket.o: $(LIB_SRC)/socket.c $(LIB_HEADERS)
	$(CC) $(CFLAGS) $(LIB_INC) -c -fpic $< -o $@

$(BUILD_SL)/sacagalib/config.o: $(LIB_SRC)/config.c $(LIB_HEADERS)
	$(CC) $(CFLAGS) $(LIB_INC) -c -fpic $< -o $@

$(BUILD_SL)/sacagalib/log.o: $(LIB_SRC)/log.c $(LIB_HEADERS)
	$(CC) $(CFLAGS) $(LIB_INC) -c -fpic $< -o $@

$(BUILD_SL)/sacagalib/children.o: $(LIB_SRC)/children.c $(LIB_HEADERS)
	$(CC) $(CFLAGS) $(LIB_INC) -c -fpic $< -o $@

$(BUILD_SL)/sacagalib/utility.o: $(LIB_SRC)/utility.c $(LIB_HEADERS)
	$(CC) $(CFLAGS) $(LIB_INC) -c -fpic $< -o $@ -lpthread

$(BUILD_SL)/sacagalib/gopher.o: $(LIB_SRC)/gopher.c $(LIB_HEADERS)
	$(CC) $(CFLAGS) $(LIB_INC) -c -fpic $< -o $@


$(BUILD_WIN_SL)/sacagalib/sacagalib.o: $(LIB_SRC)/sacagalib.c $(LIB_HEADERS)
	$(WCC) $(CFLAGS) $(LIB_INC) -c -DWIN_EXPORT $< -o $@ -lws2_32

$(BUILD_WIN_SL)/sacagalib/socket.o: $(LIB_SRC)/socket.c $(LIB_HEADERS)
	$(WCC) $(CFLAGS) $(LIB_INC) -c -DWIN_EXPORT $< -o $@ -lws2_32

$(BUILD_WIN_SL)/sacagalib/config.o: $(LIB_SRC)/config.c $(LIB_HEADERS)
	$(WCC) $(CFLAGS) $(LIB_INC) -c -DWIN_EXPORT $< -o $@ -lws2_32 -lpcre2-8 -lpcre2-posix

$(BUILD_WIN_SL)/sacagalib/log.o: $(LIB_SRC)/log.c $(LIB_HEADERS)
	$(WCC) $(CFLAGS) $(LIB_INC) -c -DWIN_EXPORT $< -o $@ -lws2_32

$(BUILD_WIN_SL)/sacagalib/children.o: $(LIB_SRC)/children.c $(LIB_HEADERS)
	$(WCC) $(CFLAGS) $(LIB_INC) -c -DWIN_EXPORT $< -o $@ -lws2_32

$(BUILD_WIN_SL)/sacagalib/utility.o: $(LIB_SRC)/utility.c $(LIB_HEADERS)
	$(WCC) $(CFLAGS) $(LIB_INC) -c -DWIN_EXPORT $< -o $@ -lws2_32

$(BUILD_WIN_SL)/sacagalib/gopher.o: $(LIB_SRC)/gopher.c $(LIB_HEADERS)
	$(WCC) $(CFLAGS) $(LIB_INC) -c -DWIN_EXPORT $< -o $@ -lws2_32


linuxserver: $(SVR_SOURCES)
	@echo 
	@echo "#####################"
	@echo "Building Linux Server"
	@echo "#####################"
	@echo 

	$(CC) $(CFLAGS) $(LIB_INC) -L${BIN} -Wl,-rpath=. -o ${BIN}/sacagawea.out $(SVR_SOURCES) -lsacagawea -lpthread -lrt

	@echo 
	@echo Linux server built successfully
	@echo 
	@echo -------------------------------
	@echo 

win32server: $(SVR_SOURCES)
	@echo 
	@echo "#####################"
	@echo "Building Win32 Server"
	@echo "#####################"
	@echo 

	$(WCC) $(CFLAGS) $(LIB_INC) -L${BIN} -Wl,-rpath=. -o ${BIN}/sacagawea.exe $(SVR_SOURCES) -lws2_32 -lsacagawea

	@echo 
	@echo Win32 server built successfully
	@echo 
	@echo -------------------------------
	@echo 


.PHONY: clean lrun wrun linux win32 linuxserver win32server linuxlib win32lib

lrun:
	cd bin && ./sacagawea.out

wrun:
	cd bin && wine ./sacagawea.exe

clean:
	rm -rf $(BIN)
	rm -rf $(BUILD)
