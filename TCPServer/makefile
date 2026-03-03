CC      := gcc
CFLAGS  := -O3 -Wall -Werror -pedantic -fPIC
LDFLAGS := -shared

LIB     := libtcpserver.so
ECHO    := echo_server

.PHONY: lib echo clean

# ---- Build shared library ----
$(LIB): TCPServer.o
	$(CC) $(LDFLAGS) -o $@ $^

TCPServer.o: TCPServer.c TCPServer.h
	$(CC) $(CFLAGS) -c TCPServer.c

# Install to system
install: $(LIB)
	sudo cp $(LIB) /usr/lib/
	sudo ldconfig
	rm -f *.o *.so a.out
# ---- Build echo server ----
echo: $(ECHO)

$(ECHO): echo_server.c $(LIB)
	$(CC) -O3 -Wall -Werror -pedantic echo_server.c -L. -ltcpserver \
	      -o $(ECHO)
	rm -f *.o *.so

# ---- Clean everything except .c and .h ----
clean:
	rm -f *.o *.so $(ECHO)
