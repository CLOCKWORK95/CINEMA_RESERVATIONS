#include "header.h"
#include "support.c"


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
    while( getchar() != '\n' );

    switch( operation) {

        case 0 :    
                    if ( strcmp( "connected.", status) == 0 ) {
                        close( socket_descriptor );
                        memset( status, 0, strlen(status) );
                        sprintf( status, "not connected.");
                        printf("\n CONNECTION CLOSED."); fflush(stdout);
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




int request_1() {


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


    //read all file and print it on terminal.

    printf("\e[1;1H\e[2J"); 
    printf("\n\n\n                                     *** CINEMA SEATS CURRENT STATE VIEW ***  \n\n");
    printf("\n\n\e[1;34m                   |.......................................................................|\e[0m      \n");
    printf(    "\e[1;34m                   |.............................  SCREEN  ................................|\e[0m      \n");
    printf(    "\e[1;34m                   |.......................................................................|\e[0m      \n\n\n");
    
    fflush(stdout);

    int counter = 0;

    while ( counter != bytes ) {

        counter += Readline( socket_descriptor,  buff, MAX_LINE );
        if (counter == -1)      Error_("error in function: read.", 1);

        printf( "%s", buff);

        fflush(stdout);

        memset( buff , 0, MAX_LINE );  

    }

    printf("\n Please, press a button to procede..."); fflush(stdout);
    while(getchar() != '\n'){}

    return 0;

}





int request_2() {

    int ret, numSeats, LINE, PLACE;        char buff[MAX_LINE];



    //set the request-code and send it to server.
    sprintf( buff, "2\n" );

    ret =  Writeline( socket_descriptor,  buff, strlen(buff));
    if (ret == -1)      Error_("error in function: Writeline.", 1);

    memset( buff, 0, strlen(buff) );



    printf("\n How many seats do you want to reserve?\n N : ");

    ret = scanf("%d", &numSeats);
    if (ret == -1)      Error_("Error in function : scanf (request_2).", 1);
    while( getchar() != '\n') {}

    printf(" Please provide seats positions :");

    for (int i = 0; i < numSeats; i ++) {

        printf("\n SEAT RESERVE N°%d :\n LINE ", i );
        ret = scanf( "%d", &LINE );
        if (ret == -1)      Error_("Error in function : scanf (request_2).", 1);
        while( getchar() != '\n') {}

        printf("\n PLACE ");
        ret = scanf( "%d", &PLACE );
        if (ret == -1)      Error_("Error in function : scanf (request_2).", 1);
        while( getchar() != '\n') {}

        sprintf( buff + strlen(buff), "%d:%d ", LINE, PLACE );

    }   sprintf(buff + strlen(buff), "\n");

    ret = Writeline( socket_descriptor, buff, strlen(buff));
    if (ret == -1)      Error_("Error in function : Writeline (request 2)", 1);

    memset( buff, 0, sizeof(buff) );

    ret = Readline( socket_descriptor, buff, MAX_LINE );
    if (ret == -1)      Error_("Error in function : Readline (request 2)", 1);

    if ( strcmp( buff, "ABORT") == 0 ) {
        printf("\n\n ABORT TRANSACTION : Unable to reserve these places for today's show.");
        fflush(stdout);
        while(getchar() != '\n'){}
        return 0;
    }

    else{
        printf(" TRANSACTION COMPLETE : Your reservation code is %s", buff );
        fflush(stdout);
        while(getchar() != '\n'){}
        return 0;
    }

}