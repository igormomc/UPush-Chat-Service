# gcc -nostartfiles -o upush_server upush_server.c for manuel testing
# gcc -nostartfiles -o upush_client upush_client.c for manuel testing
CFLAGS = -std=gnu11 -g -Wall -Wextra
#as described in the exam paper
VFLAGS = --track-origins=yes --malloc-fill=0x40 --free-fill=0x23 --leak-check=full --show-leak-kinds=all
CC=gcc
files = upush_server upush_client

# Compiles the server and the client which use send_packet.h and send_packet.c
all: $(files)

# upush_server
upush_server: upush_server.c
	$(CC) $(CFLAGS) upush_server.c send_packet.h send_packet.c -o upush_server

# upush_client
upush_client: upush_client.c
	$(CC) $(CFLAGS) upush_client.c send_packet.h send_packet.c -o upush_client

# Testing av klienter og server
testServer1: upush_server.c
	valgrind $(VFLAGS) ./upush_server 8088 0
	
testServer2: upush_server.c
	valgrind $(VFLAGS) ./upush_server 8088 1

testServer3: upush_server.c
	valgrind $(VFLAGS) ./upush_server 8088 50

testClient1: upush_client.c
	valgrind $(VFLAGS) ./upush_client alice 127.0.0.1 8088 1 0

testClient2: upush_client.c
	valgrind $(VFLAGS) ./upush_client bob 127.0.0.1 8088 2 0

testClient3: upush_client.c
	valgrind $(VFLAGS) ./upush_client jon 127.0.0.1 8088 5 1

testClient4: upush_client.c
	valgrind $(VFLAGS) ./upush_client test 127.0.0.1 8088 5 10

testClient5: upush_client.c
	valgrind $(VFLAGS) ./upush_client fred 127.0.0.1 8088 1 25

testClient6: upush_client.c
	valgrind $(VFLAGS) ./upush_client jonas 127.0.0.1 8088 1 50

testClient7: upush_client.c
	valgrind $(VFLAGS) ./upush_client donald 127.0.0.1 8088 1 75
	
clean:
	rm -f $(files)
