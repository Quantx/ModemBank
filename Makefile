
modembank: modembank.c
	gcc -g -std=gnu99 -o modembank modembank.c

clean:
	rm -f *.o modembank
