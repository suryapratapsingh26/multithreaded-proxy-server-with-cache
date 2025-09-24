CC=g++
CFLAGS= -g -Wall -std=c++17

# If on Windows (/MinGW), use windows libs
LIBS = -lws2_32

all: proxy

proxy: proxy_server_with_cache.o proxy_parse.o
	$(CC) $(CFLAGS) -o proxy $^ $(LIBS)

proxy_server_with_cache.o: proxy_server_with_cache.cpp proxy_parse.h
	$(CC) $(CFLAGS) -c proxy_server_with_cache.cpp

proxy_parse.o: proxy_parse.cpp proxy_parse.h
	$(CC) $(CFLAGS) -c proxy_parse.cpp

clean:
	-rm -f proxy *.o proxy.exe

tar:
	tar -cvzf proxy-server.tgz proxy_server_with_cache.cpp proxy_parse.cpp proxy_parse.h README.md Makefile.mk
