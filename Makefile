all: server client

server: src/server.c 
	gcc -o bin/server src/server.c -lm

client: src/client.c 
	gcc -o bin/client src/client.c -lm

clean:
	rm -f bin/server bin/client

run-server:
	bin/server v4 51511
run-client-temperatura:
	bin/client 127.0.0.1 51511 -type temperature -coords 2 3
run-client-umidade:
	bin/client 127.0.0.1 51511 -type humidity -coords 4 5
run-client-ar:
	bin/client 127.0.0.1 51511 -type air_quality -coords 1 1


git-update:
	git stash
	git pull
	make all
