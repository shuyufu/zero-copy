all: server client

server:
	gcc -Wall -o server server.c `pkg-config --cflags --libs glib-2.0 gio-2.0`

client:
	gcc -Wall -o client client.c `pkg-config --cflags --libs glib-2.0 gio-2.0`

clean:
	rm -f server client
