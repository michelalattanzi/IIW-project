makeall:
	gcc -g -o client client.c -lpthread
	gcc -g -o server server.c -lpthread

cleanall:
	rm client
	rm server