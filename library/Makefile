CFLAGS = -L. -lfinger.so
CC = gcc
all: main finger.so
	
finger.so: 
	${CC} -fPIC -shared finger.h -o finger.so

main: main.c finger.h finger.so
	${CC} ${CFLAGS} -o $@ $^