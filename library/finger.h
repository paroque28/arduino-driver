
#ifndef _ARDUINO_LIBRARY_H
#define _ARDUINO_LIBRARY_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

FILE *_dev;   
char *_device;

//Set device file
int set_device(char *device ) {
	//Open device file
	_dev = fopen(device, "r+");
	if (!_dev)	{
		//Returns false if failed
		printf("Error opening device\n");
		return 0;
	} 
	else  {
		//Set device if successful
		fclose(_dev);
		printf("Successfully opened device!\n");
		_device = device;
		return 1;
	}
}

size_t write_to_device(char* string, size_t size)  {
	_dev = fopen(_device, "r+");
	if(_dev != NULL){
		fwrite((const void *)(string), size, 1, _dev);
		fclose(_dev);
		return size;
	}
	else 	{
		printf("I/O Error\n");
		return -1;
	}
}


void move(int x, int y)  {
	char *message = (char*)malloc(8 * sizeof(char));
	sprintf(message, "m%d,%d\n",x,y);
	write_to_device(message,8);
}


void pick()  {
	char *message = (char*)malloc(2 * sizeof(char));
	sprintf(message, "p\n");
	write_to_device(message,2);
}


void drop()  {
	char *message = (char*)malloc(2 * sizeof(char));
	sprintf(message, "d\n");
	write_to_device(message,2);
}


#endif