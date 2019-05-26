
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

void write_to_device(char* string, size_t size)  {
	//Open device file
	_dev = fopen(_device, "r+");
	if(_dev != NULL){
		//Send coordinates
		fwrite((const void *)(string), size, 1, _dev);
		fclose(_dev);
		printf("Message sent: %s\n", string);
	}
	else 	{
		printf("I/O Error\n");
	}
}


#endif