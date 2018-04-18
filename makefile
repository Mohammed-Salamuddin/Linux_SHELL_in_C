all: mr
mr: mr.o
	gcc -o mr mr.o -g -lreadline
mr.o: mr.c
	gcc -c mr.c 
clean:
	rm -f mr.o
