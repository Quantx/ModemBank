
modembank: modembank.c
	gcc -g -std=gnu99 -o modembank modembank.c shell.c commands.c

clean:
	rm -f *.o modembank
