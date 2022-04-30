# gcc -nostartfiles -o server server.c
# gcc -nostartfiles -o client client.c

# ./server

cc=gcc
flags=-Wall
clientLibraries=-pthread
flagsFormat=-sob -bad -bap -br -nce -cli4 -npcs -cs -nsaw -nsai -nsaf -nbc -di1 -cdb -sc -brs -brf -i4 -lp -ppi 4 -l100 --ignore-newlines -nbbo -nut -ncdw -nbc -npsl
files=server.c 
serverOut=server
clientOut=client


all: compile

compile:client server

client: $(source)client.c
	$(cc) $(flags) $(clientLibraries) $(source)client.c -o $(clientOut)

server: $(source)server.c
	$(cc) $(flags) server.c -o $(serverOut)

clean:
	rm $(serverOut) rm $(clientOut)

format:
	indent $(files) $(flagsFormat)

