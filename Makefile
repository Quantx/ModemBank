
modembank: modembank.c shell.c commands.c connections.c
	gcc -g -std=gnu99 -o modembank modembank.c shell.c commands.c connections.c

clean:
	rm -f *.o modembank
