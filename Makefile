server: server.o
	gcc -o server server.o

client: client.o data.o resources.o
	gcc $^ -Llib -lraylib -lGL -lm -lpthread -ldl -lX11 -o client

client.o: client.c data.h resources.h
	gcc -g -c client.c -Iinclude -o client.o

resources.o: resources.c data.h
	gcc -c resources.c -Iinclude -o resources.o

data.o: data.c
	gcc -c data.c -Iinclude -o data.o
	
clean:
	rm -f *.o server client
