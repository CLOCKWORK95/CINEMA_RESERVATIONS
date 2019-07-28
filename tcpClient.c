#include <sys/socket.h>       /*  socket definitions        */
#include <sys/types.h>        /*  socket types              */
#include <arpa/inet.h>        /*  inet (3) funtions         */
#include <unistd.h>           /*  misc. UNIX functions      */
#include <netinet/in.h>
#include <netdb.h>
#include "support.h" 
#include "support.c"     
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

    // printf("\e[1;34m This is a blue text.\e[0m \n\n");
    // printf("\e[0;31m goodmorning vietnam!\e[0m \n");


struct sockaddr_in      server_address;

int                     socket_descriptor;

char                    status[16] = "not connected.";

                          




int main (int argc, char** argv) {
    
    int ret;

gui:

    display();

    int operation;

    printf("\n\n     Insert operation code here : ");
    ret = scanf( "%d", &operation);
    if (ret == -1)      Error_("error in function: scanf.", 1);
    while( getchar() != '\n');

    switch( operation) {

        case 0 :    
                    if ( strcmp( "connected.", status) == 0 ) {
                        printf("\n You're already connected to server!");
                        sleep(1);
                        goto gui; }

                    else    try_connection();

                    goto gui;

        case 1 :
                    request_1();
                    goto gui;

        case 2:     
                    request_2();
                    goto gui;

        default:    goto gui;

    }


    return EXIT_SUCCESS;

}





void display() {

    printf("\e[1;1H\e[2J");
    printf("....................................................................................\n");
    printf("....................................................................................\n");
	printf("..........................|      CINEMA RESERVATIONS      |.........................\n");
    printf("....................................................................................\n");
	printf("....................................................................................\n\n");
    printf(" STATUS : %s \n", status);
    printf(" CTRL + C to close connection. \n\n\n");

	printf(" _____ ________ ________ ____What are you looking for ?____ ________ _______ _______\n");
	printf("|                                                                                   |\n");
    printf("|   OP   0 : Connect to CINEMA RESERVATIONS SERVER.                                 |\n");
    printf("|   OP   1 : View the current state of today's seats reservation.                   |\n");
    printf("|   OP   2 : Reserve some seats for today's show.                                   |\n");
	printf("|____ ________ ________ ________ ________ ________ ________ ________ ________ ______|\n\n");

    fflush(stdout);

}



void try_connection() {

    int ret;

    //open a session on a socket, identified by socket_descriptor.
    socket_descriptor = socket( AF_INET, SOCK_STREAM, 0);
    if (socket_descriptor == -1)        Error:("Error in function: socket.", 1);

    //set contents of sockaddr_in structure representing server's address.
    memset( &server_address, 0, sizeof( struct sockaddr ));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    //try to connect to the server.
    ret = connect( socket_descriptor, (struct sockaddr *) &server_address, sizeof( server_address ) );
    if (ret == -1 )         Error_("error in function: connect.", 1);

    memset(status, 0, strlen(status));
    strcpy(status, "connected.");

}




int request_1(){


    char buff[MAX_LINE];        int ret,        bytes = 0;



    //set the request-code and send it to server.
    sprintf( buff, "1\n" );

    ret =  Writeline( socket_descriptor,  buff, strlen(buff) );
    if (ret == -1)      Error_("error in function: Writeline.", 1);

    memset( buff, 0, strlen(buff) );



    //get file size from server.
    ret = Readline( socket_descriptor, buff, MAX_LINE);
    if (ret == -1)      Error_("error in function: Readline.", 1);

    bytes = atoi( buff );

    memset( buff, 0, strlen(buff) );



    //ACK / NACK the server on received file-lines number.
    if ( bytes != 0) {

        sprintf( buff, "ACK\n");

        ret = Writeline( socket_descriptor, buff, strlen(buff));
        if (ret == -1)      Error_("error in function : Writeline.", 1);

        memset( buff, 0, strlen(buff) );

    }   else {

        sprintf( buff, "NACK\n");

        ret = Writeline( socket_descriptor, buff, strlen(buff));
        if (ret == -1)      Error_("error in function : Writeline.", 1);

        memset( buff, 0, strlen(buff) );

        printf("\nnothing to read...  ");

        goto end;

    }
    


    //read all file and print it on terminal.

    printf("\e[1;1H\e[2J"); 
    printf("\n\n\n                                     *** CINEMA SEATS CURRENT STATE VIEW ***  \n\n");
    printf("\n\n\e[1;34m                   |.......................................................................|\e[0m      \n");
    printf(    "\e[1;34m                   |.............................  SCREEN  ................................|\e[0m      \n");
    printf(    "\e[1;34m                   |.......................................................................|\e[0m      \n\n\n");
    
    fflush(stdout);

    int counter = 0;

    while ( counter != bytes ) {

        counter += read( socket_descriptor,  buff, MAX_LINE );
        if (counter == -1)      Error_("error in function: read.", 1);

        printf( "%s", buff);

        fflush(stdout);

        memset( buff , 0, MAX_LINE );  

    }

 end:

    quit();

    return 0;

}





int request_2(){}