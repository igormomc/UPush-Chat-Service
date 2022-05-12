# gcc -nostartfiles -o server server.c
# gcc -nostartfiles -o client client.c 
CFLAGS = -std=gnu11 -g -Wall -Wextra
#as described in the exam paper
VFLAGS = --track-origins=yes --malloc-fill=0x40 --free-fill=0x23 --leak-check=full --show-leak-kinds=all
files = upush_server upush_client

# Compiles the server and the client which use send_packet.h and send_packet.c
all: $(files)

# upush_server
upush_server: upush_server.c
	gcc $(CFLAGS) upush_server.c send_packet.h send_packet.c -o upush_server

# upush_client
upush_client: upush_client.c
	gcc $(CFLAGS) upush_client.c send_packet.h send_packet.c -o upush_client

# Testing av klienter og server
testServer1: upush_server.c
	valgrind $(VFLAGS) ./upush_server 8085 0
	
testServer2: upush_server.c
	valgrind $(VFLAGS) ./upush_server 8085 1

testServer3: upush_server.c
	valgrind $(VFLAGS) ./upush_server 8085 50

testClient1: upush_client.c
	valgrind $(VFLAGS) ./upush_client alice 127.0.0.1 8085 1 0

testClient2: upush_client.c
	valgrind $(VFLAGS) ./upush_client bob 127.0.0.1 8085 1 1

testClient3: upush_client.c
	valgrind $(VFLAGS) ./upush_client jon 127.0.0.1 8085 5 1

testClient4: upush_client.c
	valgrind $(VFLAGS) ./upush_client test 127.0.0.1 8085 5 5

testClient5: upush_client.c
	valgrind $(VFLAGS) ./upush_client fred 127.0.0.1 8085 1 15

testClient6: upush_client.c
	valgrind $(VFLAGS) ./upush_client jonas 127.0.0.1 8085 1 25

clean:
	rm -f $(files)
