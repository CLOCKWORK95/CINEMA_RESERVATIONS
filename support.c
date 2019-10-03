#pragma once
#include "header.h"
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>




//read a line from socket_descriptor channel, putting its content in vptr.
ssize_t Readline( int socket_descriptor, void *vptr, size_t maxlenght ) {
    
    ssize_t     n,      read_character;
    char        c,          *buffer;

    buffer = vptr;

    for ( n = 1; n < maxlenght; n++ ) {

		read_character = read(socket_descriptor, &c, 1);

		if ( read_character == 1 ) {        //a character has been read.
	    		*buffer++ = c;
	    		if ( c == '\n' ) break;
				if ( c == '\0' ) break;
		}
		else
			if ( read_character == 0 ) {    
			    if ( n == 1 ) return 0;     //nothing to read.
			    else break;
			}
			else {
			    if ( errno == EINTR ) continue;
			    return -1;
			}
    }

    *buffer = 0;

    return n;       //line lenght.

}



//write on socket descriptor channel.
ssize_t Writeline( int socket_descriptor , const void *vptr, size_t n ) {

    size_t      nleft;          ssize_t     nwritten;       const char *buffer;

    buffer = vptr;
    nleft  = n;

    while ( nleft > 0 ) {       //make sure of writing all characters on socket.

		if ( ( nwritten = write( socket_descriptor, buffer, nleft ) ) <= 0 ) {
            if ( errno == EINTR ) nwritten = 0;
		    else return -1;

		}

		nleft  -= nwritten;
		buffer += nwritten;

    }

    return n;
}




int Lines_counter( FILE * f ) {

	int 	ret,	num_lines = 0;		

	char 	*line = NULL;

	size_t 	len = 0;

	while ( getline( &line , &len , f) != -1 ) {

		num_lines ++;

	}

	return num_lines;

}



int Writefile( int socket_descriptor, FILE * stream ) {

	int 	size = 0;

	char 	*l = NULL;

	size_t 	len = 0;


	while ( getline( &l, &len, stream) != -1 ) {

		size += Writeline( socket_descriptor, l, strlen(l) );
		if (size == -1)		return -1;

	} 

	return size;

}




typedef struct seat_{

    int             line;
    int             place;
    char            is_reserved; 
	struct seat_	*next;     

} seat;