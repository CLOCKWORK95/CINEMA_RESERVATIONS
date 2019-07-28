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
#include <pthread.h>
#include <semaphore.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <sys/stat.h>


#define MAX_CONNECTIONS 10

#define MAX_PENDING 5


int     socket_descriptor,       num_conns = 0;

struct sockaddr_in      my_address,     their_address;
                                                        

//define linked list for dynamic memory allocation of client-server connections.
typedef struct concurrent_connection_ {

    int                                     sock_descr;
    int                                     connection_number;
    pthread_t                               tid;
    struct concurrent_connection_           *next;

} concurrent_connection ;                                   concurrent_connection   *new_conn;           concurrent_connection   *tmp;


__thread int    descr,       my_conn_num,       file_descriptor;    

__thread FILE   *f;

__thread seat   *seats_vector,      *reservation_seats;





void * connection_handler ( void * attr);


void send_seats_view();




int main (int argc, char** argv){

    int ret,    addrlen;

    //open a session on a socket.
    socket_descriptor = socket( AF_INET, SOCK_STREAM, 0);
    if (socket_descriptor == -1)        Error_("error in function: socket.", 1);

    //initiate the address of this server-socket, and then bind it to the opened session.
    memset( &my_address, 0, sizeof( my_address));
    my_address.sin_family = AF_INET;
    my_address.sin_port = htons(PORT);
    my_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    ret = bind( socket_descriptor, (struct sockaddr *) &my_address, sizeof(my_address) );
    if (ret != 0)      Error_("error in function: bind.", 1);

    //connection-oriented socket has to be put on listening to accept one or more connection requests.
    ret = listen( socket_descriptor, MAX_PENDING);
    if (ret == -1)      Error_("error in function: listen.", 1);

    addrlen = sizeof( their_address );

    printf("        *** CINEMA RESERVATIONS SERVER ***   ");
    printf("\n\n    listening to connection requests...\n");

    //requests handler loop.
    while ( num_conns <= MAX_CONNECTIONS ) {


        ret = accept( socket_descriptor, (struct sockaddr *) &their_address, &addrlen );
        if (ret == -1)      Error_("error in function: accept.", 1);


            {   //generate a new child thread to handle the new client-server connection (server side).

                num_conns ++;

                new_conn = malloc( sizeof( concurrent_connection ) );
                if (new_conn == NULL)        Error_("error in function: malloc.", 1);

                memset( new_conn, 0, sizeof(concurrent_connection) );
                new_conn -> sock_descr = ret;
                new_conn -> connection_number = num_conns;
                
                tmp = new_conn;
                
                ret = pthread_create( &( tmp -> tid ), NULL, connection_handler, (void *) tmp ); 
                if (ret == -1)      Error_("error in function: pthread_create.", 1);

                new_conn = new_conn -> next;

            }


    }


    do  {     pause();    }       while(1);


    return EXIT_SUCCESS;
    
}




void * connection_handler (void * attr) {

    int ret,    request_code;

    char buffer[MAX_LINE];

    //set the TLS.
    descr = (int) ( (concurrent_connection *) attr ) -> sock_descr;
    my_conn_num = (int) ( (concurrent_connection *) attr ) -> connection_number;

    //open a session on file "cinema", to serve client requests.
    file_descriptor = open( "cinema", O_RDWR , 0660 );
    if (file_descriptor == -1)      Error_("error in function: open.", 1);

    f = fdopen( file_descriptor, "r+" );
    if (f == NULL)      Error_("error in function: fdopen (send seats view).", 1);

    //print connection infos on screen.
    printf("\n\n\nThis is thread n° %ld of CINEMA RESERVATION SERVER.", pthread_self() );
    printf("\nManaging connection on socket n° %d .", descr);
    printf("\nConnection n° %d established : Ready to answer client requests...", my_conn_num );
    fflush(stdout);

 
    do{

        ret = Readline( descr, buffer, MAX_LINE );
        if (ret == -1)      Error_("error in function : Readline.", 1);
        
        if (ret != 0){

            request_code = atoi( buffer);
            printf("\nrequest code : %d  -  ", request_code);
            fflush(stdout);

            switch(request_code) {

                case 1:
                        send_seats_view();
                        break;

                case 2:
                        break;
                
                default:
                        printf("\ndefault!\n");
                        break;
            }
        }

    } while(1);
            

}




void send_seats_view() {

    printf("serving client request on socket %d.\n", descr);

    int     ret,    size;

    char    line[MAX_LINE];


    //get the file size.
    size = (int) lseek( file_descriptor, 0, SEEK_END);  
    lseek( file_descriptor, 0, SEEK_SET );  
    printf("  -  size of file : %d ", size);  
    fflush(stdout);



    //send number of bytes of file to the client.
    ret = sprintf( line, "%d\n", size );
    if (ret == -1)      Error_("error in function: sprintf (send seats view).", 1);

    ret = Writeline( descr, line, strlen(line));
    if (ret == -1)      Error_("error in function: Writeline (send seats view).", 1);

    memset( line, 0 , strlen(line) );



    //check the client has received a non null number of lines. 
    ret = Readline( descr, line, MAX_LINE );
    if (ret == -1)      Error_("error in function : Readline", 1);

    if ( strcmp( line, "NACK\n") == 0 ) goto end;

    if ( strcmp( line, "ACK\n") == 0) {

        printf(" - sending all file to client... ");
                                                                                                               
        //send all file to the client.
        ret = Writefile( descr, f );
        if (ret == -1)      Error_("error in function: Writeline (send seats view).", 1);

        printf(" - sent %d bytes.", ret );

    }


  end:

    printf("\nrequest served on socket %d.", descr);
    fflush(stdout);
    
    return;

}




void reserve_and_confirm(){

    //validate memory TLS to represent all seats' informations in cinema hall.
    seats_vector = malloc( sizeof(seat) * 10);
    if (seats_vector == NULL)       Error_("error in function : malloc.", 1);

    char    *l = NULL,      delimiter[8],       line[15],        *token;

    int     ret;
    
    size_t  len = 0;


    int line_counter = 1;

    seat *tmp = NULL;


    /*  by this cycle, parse the 'cinema' file to build linked list structs (seat) 
        containing all current informations of cinema hall. */

    while( getline(&l, &len, f) != -1 &&  line_counter < 11 ) {

        //dinamically set delimiter string's value.
        ret = sprintf( delimiter, "FILA%d ", line_counter);
        if (ret == -1)      Error_("error in function : sprintf", 1);


        //build a 'line string' containing current reservation values for each place of this (line_counter) line.
        strcat( line, strtok( l, delimiter ) );
        while ( ( token = strtok(NULL, " ") ) != NULL ) {
            strcat( line, token );
        }


        //set all seats_vector[line_counter] infos.
        tmp = seats_vector + line_counter;

        for (int i = 0; i < strlen(line); i ++) {

            tmp -> is_reserved = line[i];
            tmp -> line = line_counter;
            tmp -> place = i;
            tmp -> next = malloc( sizeof( seat ));
            if (tmp -> next == NULL)        Error_("error in function : malloc", 1);
            tmp = tmp -> next;

        }

        free(tmp);

    }


    /*  in this code block further seat structs are allocated and filled, 
        to represent the input ( seats reserving request)from client side. */

    char buffer[MAX_LINE];

    ret = Readline( descr, buffer, MAX_LINE );
    if (ret == -1)      Error_("error in function : Readline.", 1);

    memset( line, 0, strlen(line) );

    reservation_seats = malloc(sizeof(seat));

    tmp = reservation_seats;

    tmp -> line = atoi(strtok( buffer, ":"));
    tmp -> place = atoi(strtok( NULL, " "));
    tmp -> is_reserved = '1';
    tmp -> next = malloc( sizeof(seat));
    if (tmp -> next == NULL)     Error_("error in function : malloc.", 1); 
    tmp = tmp -> next;

    while ( ( token = strtok( NULL, ":") ) != NULL) {

        tmp -> line = atoi(token);
        tmp -> place = atoi(strtok( NULL, " "));
        tmp -> is_reserved = '1';
        tmp -> next = malloc( sizeof(seat));
        tmp = tmp -> next;

    }

    free(tmp);


    //here's implemented the confirm / negation to client on his seats-reserving request.

    char confirm;

    while ( reservation_seats -> next != NULL ) {

        tmp = reservation_seats;

        int     _line = tmp -> line;
        int     _place = tmp -> place;

        tmp = seats_vector + (_line - 1);
        for (int i = 0; i < _place; i ++) {
            tmp = tmp -> next;
        }

        if ( tmp -> is_reserved == '0')     confirm = '1';

        else {
            confirm = '0';
            break;
        }

    }

    if (confirm == '1') {
        //override file with new values, then send back a reservation confirm.

    } else{
        //send back a reservation negation.
    }




}