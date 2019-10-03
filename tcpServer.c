#include "header.h"
#include "support.c"


int                     socket_descriptor,       num_conns = 0;

//mutex for synchronized threads' access on files 'cinema' and 'cinema_prenotazioni'.
pthread_mutex_t         CINEMA_MUTEX = PTHREAD_MUTEX_INITIALIZER;

struct sockaddr_in      my_address,     their_address;
                                                        

//define linked list for dynamic memory allocation of client-server connections.
typedef struct concurrent_connection_ {

    int                                     sock_descr;
    int                                     connection_number;
    pthread_t                               tid;
    struct concurrent_connection_           *next;

} concurrent_connection ;                                   concurrent_connection   *new_conn;           concurrent_connection   *tmp;


__thread int    descr,       my_conn_num,       file_descriptor;    

__thread seat   *seats_vector,                  *reservation_seats;



void * connection_handler ( void * attr);

void send_seats_view();

void reserve_and_confirm();

void delete_reservation();

void build_cinema();

void build_reservation_seats();

int get_cancellation_seats();





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
    while ( 1 ) {


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


    return EXIT_SUCCESS;
    
}




void * connection_handler (void * attr) {

    int ret,    request_code;

    char buffer[MAX_LINE];

    //set the TLS.
    descr = (int) ( (concurrent_connection *) attr ) -> sock_descr;
    my_conn_num = (int) ( (concurrent_connection *) attr ) -> connection_number;


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
                        reserve_and_confirm();
                        break;

                case 3:
                        delete_reservation();
                        break;
                
                default:
                        printf("\ndefault!\n");
                        sleep(3);
                        break;
            }
        }

    } while(1);
            

}




void send_seats_view() {

    printf("serving client request on socket %d.\n", descr);

    int     ret,    size;

    char    line[MAX_LINE];

    FILE    *f;

    pthread_mutex_lock( &CINEMA_MUTEX );

    //open a session on file "cinema", to serve client requests.

    f = fopen( "cinema", "r+" );
    if (f == NULL)      Error_("error in function: fdopen (send seats view).", 1);


    //get the file size and add a newline at the end of it.
    fseek( f , 0, SEEK_END);  
    size = ftell( f );
    fseek( f , 0, SEEK_SET );  
    printf("  -  size of file : %d ", size);  
    fflush(stdout);


    //send number of bytes of file to the client.
    ret = sprintf( line, "%d\n", size );
    if (ret == -1)      Error_("error in function: sprintf (send seats view).", 1);

    ret = Writeline( descr, line, strlen(line));
    if (ret == -1)      Error_("error in function: Writeline (send seats view).", 1);

    memset( line, 0 , strlen(line) );


    printf(" - sending all file to client... ");
                                                                                                            
    //send all file to the client.
    ret = Writefile( descr, f );
    if (ret == -1)      Error_("error in function: Writeline (send seats view).", 1);

    printf(" - sent %d bytes.", ret );

    end:

    printf("\nrequest served on socket %d.", descr);
    fflush(stdout);

    fclose( f );

    pthread_mutex_unlock( &CINEMA_MUTEX );
    
    return;

}




void reserve_and_confirm() {

    //validate memory TLS to represent all seats' informations in cinema hall.
    seats_vector = malloc( sizeof(seat) * 9);
    if (seats_vector == NULL)       Error_("error in function : malloc.", 1);

    char    *l = NULL,      *token;

    int     ret,            line_counter = 1;   size_t len;

    seat    *tmp = NULL,       *last = NULL;

    FILE    *f;

    //ensure the atomicity of transaction.
    pthread_mutex_lock( &CINEMA_MUTEX ); 

    f = fopen( "cinema" , "r+" );
    if (f == NULL)      Error_("error in function: fopen (send seats view).", 1);

    build_cinema( f );

    //allocate seat_structures to represent the input of this reservation occurrance.
    char buffer[MAX_LINE];

    ret = Readline( descr, buffer, MAX_LINE );
    if (ret == -1)      Error_("error in function : Readline.", 1);

    printf("\n Buffer content : %s", buffer);       fflush(stdout);

    build_reservation_seats( buffer );

    //here's implemented the confirm / negation to client on his seats-reserving request.

    char confirm;

    seat *first = reservation_seats;

    do {

        int     _line = reservation_seats -> line;
        int     _place = reservation_seats -> place;

        tmp = seats_vector + (_line - 1);

        for (int i = 0; i < _place -1; i ++) {
            tmp = tmp -> next;
        }

        if ( tmp -> is_reserved == '0')     confirm = '1';

        else {
            confirm = '0';
            break;
        }

        if( reservation_seats -> next == NULL)      break;

    } while ( ( reservation_seats = reservation_seats -> next ) != NULL ); 


    reservation_seats = first;

    if (confirm == '1') {

        //override file with new values, then send back a reservation confirm.

        do {

            int     _line = reservation_seats -> line;
            int     _place = reservation_seats -> place;

            tmp = seats_vector + (_line - 1);
            for (int i = 0; i < _place -1 ; i ++) {
                tmp = tmp -> next;
            }

            if ( tmp -> is_reserved == '0')     tmp -> is_reserved = 'X';

            if( reservation_seats -> next == NULL)      break;

        } while ( ( reservation_seats = reservation_seats -> next ) != NULL );


        //write updates on file 'cinema'

        memset( buffer, 0, sizeof(buffer) );

        for( int i = 0; i < 9; i ++ ) { 
            
            //override file contents with updated ones.

            tmp = seats_vector + i;

            sprintf( buffer + strlen(buffer), "FILA%d           ",  tmp -> line );

            for(int j = 0; j < 14; j ++) {

                if( j == 13 )   sprintf( buffer + strlen(buffer), "%c\n\n\n", tmp -> is_reserved );
                else            sprintf( buffer + strlen(buffer), "%c     ",  tmp -> is_reserved );

                tmp = tmp -> next;

            } 

        }

        fseek( f, 0, SEEK_SET);
        ret = fputs( buffer, f );
        if (ret == -1)      Error_("Error in function : fputs (reserve and confirm)", 1);

        fflush( f );              fclose( f );



        //complete reservation procedure by sending the client his own reservation code.

        int fd = open( "cinema_prenotazioni", O_RDWR, 0660 );
        if (fd == -1)       Error_("Error in function : open (reserve and confirm)", 1);

        int reservation_code = (int) lseek(fd, 0, SEEK_END);

        tmp = first;

        memset( buffer, 0, sizeof(buffer) );

        sprintf( buffer, "%d/", reservation_code);

        do {

            sprintf( buffer + strlen(buffer), "%d:%d ", tmp -> line, tmp -> place );

        } while( ( tmp = tmp -> next ) != NULL );

        sprintf( buffer + strlen(buffer), "\n" );


        write( fd, buffer, strlen(buffer) );
        if (ret == -1)      Error_("Error in function : write (reserve and confirm", 1);

        printf("TRANSACTION COMPLETE. Reservation infos : %s", buffer );fflush(stdout);

        close(fd);

        //release mutex.
        pthread_mutex_unlock( &CINEMA_MUTEX );


        memset( buffer, 0, sizeof(buffer) );
        sprintf( buffer, "%d", reservation_code );
        
        ret = Writeline( descr, buffer, strlen(buffer) + 1 );
        if (ret == -1)      Error_("Error in function : Writeline (reserve and confirm).", 1);




    } else{
        //release mutex.
        pthread_mutex_unlock( &CINEMA_MUTEX );

        memset( buffer, 0, strlen(buffer) );
        sprintf( buffer, "ABORT" );
        
        //send back a reservation negation.
        ret = Writeline( descr, buffer, strlen(buffer) + 1 );
        if (ret == -1)      Error_("Error in function : Writeline (reserve and confirm).", 1);

        printf("\n TRANSACTION ABORTED. Error in retriving seats for the show."); fflush(stdout);

    }

    free(seats_vector);
    free(reservation_seats);
    

}




void delete_reservation() {

    //validate memory TLS to represent all seats' informations in cinema hall.
    seats_vector = malloc( sizeof(seat) * 9);
    if (seats_vector == NULL)       Error_("error in function : malloc.", 1);

    char    *l = NULL,      *token;

    int     ret; 
    
    size_t len;

    seat    *tmp = NULL,       *last = NULL;

    FILE    *f;

    //ensure atomicity of transaction.
    pthread_mutex_lock( &CINEMA_MUTEX ); 

    f = fopen( "cinema" , "r+" );
    if (f == NULL)      Error_("error in function: fopen (send seats view).", 1);

    build_cinema( f );

    //get the reservation code from the client.
    char reservation_code[MAX_LINE];

    ret = Readline( descr, reservation_code, MAX_LINE );
    if (ret == -1)      Error_("error in function : Readline.", 1);

    printf("\n Buffer content : %s", reservation_code);       fflush(stdout);

    //find the code within reservation file and procede with cancellation.
    FILE *codes = fopen( "cinema_prenotazioni", "r+" );

    int bytes = 0;
    int codelen = 0;
    sleep(1);
    
    ret = get_cancellation_seats( codes, reservation_code, &bytes, &codelen );

    if( ret != 0 )   goto end2;

    fseek( codes, (bytes - codelen), SEEK_SET);

    fputs( "NV", codes );

        //override 'cinema' file with updated reservation values.
    do {
        int     _line = reservation_seats -> line;
        int     _place = reservation_seats -> place;

        tmp = seats_vector + (_line - 1);
        for (int i = 0; i < _place -1 ; i ++) {
            tmp = tmp -> next;
        }

        if ( tmp -> is_reserved == 'X')     tmp -> is_reserved = '0';

        if( reservation_seats -> next == NULL)      break;

    } while ( ( reservation_seats = reservation_seats -> next ) != NULL );

    char buffer[MAX_LINE];

    memset(buffer, 0, sizeof(buffer));

    for( int i = 0; i < 9; i ++ ) { 

        tmp = seats_vector + i;

        sprintf( buffer + strlen(buffer), "FILA%d           ",  tmp -> line );

        for(int j = 0; j < 14; j ++) {

            if( j == 13 )   sprintf( buffer + strlen(buffer), "%c\n\n\n", tmp -> is_reserved );
            else            sprintf( buffer + strlen(buffer), "%c     ",  tmp -> is_reserved );

            tmp = tmp -> next;

        } 
    }
    fseek( f, 0, SEEK_SET);
    ret = fputs( buffer, f );
    if (ret == -1)      Error_("Error in function : fputs (reserve and confirm)", 1);

    fflush( f );              fclose( f );

    //release the mutex.
    pthread_mutex_unlock( &CINEMA_MUTEX );
    fclose( codes );
    free( reservation_seats );
    free( seats_vector );
    printf("\n TRANSACTION COMPLETE. Reservation has been successfully deleted."); fflush(stdout);
    return;

    end2:
    pthread_mutex_unlock( &CINEMA_MUTEX );
    fclose( codes );
    free( reservation_seats );
    free( seats_vector );
    printf("\n Reservation not found."); fflush(stdout);
    return;

}




void build_cinema( FILE *f ) {

    //validate 'seats_vector' TLS to represent all seats' informations in cinema hall.

    seats_vector = malloc( sizeof(seat) * 9);
    if (seats_vector == NULL)       Error_("error in function : malloc.", 1);

    char    *l = NULL,      delimiter[10],       *line,        *token;

    int     line_counter = 1;                   
    
    size_t len;

    seat    *tmp = NULL,       *last = NULL;

    line = malloc( sizeof(char) * 16 );
    if (line == NULL)       Error_("Error in function : malloc. (build_cinema).", 1);

    /*  by this cycle, parse the 'cinema' file to build linked list structs (seat) 
        containing all current informations of cinema hall. */

    while( getline(&l, &len, f) != -1 &&  line_counter < 10 ) {

        memset( line, 0, strlen(line) );

        if ( strcmp( l, "\n") == 0 )    goto redo;

        //dinamically set delimiter string's value.
        sprintf( delimiter, "FILA%d  ", line_counter);

        //build a 'line string' containing current reservation values for each place of this (line_counter) line.
        token = strtok( l, delimiter );
        strcat( line, token );
        memset( token, 0, strlen(token));
        while ( ( token = strtok(NULL, " ") ) != NULL ) {
            strcat( line, token );
            memset( token, 0, strlen(token));
        }

        printf("\nline: %s", line);fflush(stdout);

        //set all seats_vector[line_counter] infos.
        tmp = seats_vector + ( line_counter - 1 );

        seat    *last_seat = NULL;

        for (int i = 0; i < strlen(line); i ++) {

            tmp -> is_reserved = line[i];
            tmp -> line = line_counter;
            tmp -> place = i + 1;
            tmp -> next = malloc( sizeof( seat ));
            if (tmp -> next == NULL)        Error_("error in function : malloc", 1);
            last_seat = tmp;
            tmp = tmp -> next;

        }   last_seat -> next = NULL;   free(tmp);

        line_counter ++;

        redo:

        memset( line, 0, strlen(line) );

    }   free(line);

}


void build_reservation_seats( char * buffer) {

    char * token = NULL;

    seat *tmp,      *last;

    reservation_seats = malloc(sizeof(seat));
    if (reservation_seats == NULL) Error_(" Error in function : malloc. (reserve and confirm)", 1);

    tmp = reservation_seats;

    token = strtok( buffer, ":" );
    tmp -> line = atoi( token );
    memset( token, 0, strlen(token));

    token = strtok( NULL, " ");
    tmp -> place = atoi( token );
    memset( token, 0, strlen(token));

    tmp -> next = malloc( sizeof(seat));
    if (tmp -> next == NULL)     Error_("error in function : malloc.", 1); 
    last = tmp;
    tmp = tmp -> next;

    while ( ( token = strtok( NULL, "\n:") ) != NULL ) {

        tmp -> line = atoi(token);
        memset( token, 0, strlen(token));

        token = strtok( NULL, " ");
        tmp -> place = atoi( token );
        memset( token, 0, strlen(token));

        tmp -> next = malloc( sizeof(seat));
        last = tmp;
        tmp = tmp -> next;

    }   
    
    if (last != NULL) {
        last -> next = NULL;      
        free(tmp);
    } 

}


int get_cancellation_seats( FILE * codes, char * reservation_code, int *bytes, int *codelen ) {

    char    *l,     *token = NULL;   

    size_t          len;

    seat    *tmp,   *last;


    while ( getline( &l, &len, codes ) != -1 ) {

        *bytes += strlen(l);
        *codelen = strlen(l);

        token = strtok( l, "/");

        if( strcmp( token, reservation_code ) == 0 ) {


            reservation_seats = malloc(sizeof(seat));
            if (reservation_seats == NULL) Error_(" Error in function : malloc. (get_cancellation_seats)", 1);

            tmp = reservation_seats;

            token = strtok( NULL, ":" );
            tmp -> line = atoi( token );
            memset( token, 0, strlen(token));

            token = strtok( NULL, "\n ");
            tmp -> place = atoi( token );
            memset( token, 0, strlen(token));

            tmp -> next = malloc( sizeof(seat));
            if (tmp -> next == NULL)     Error_("error in function : malloc.", 1); 
            last = tmp;
            tmp = tmp -> next;

            while ( ( token = strtok( NULL, ":\n") ) != NULL ) {

                tmp -> line = atoi(token);
                memset( token, 0, strlen(token));
                
                token = strtok( NULL, " \n");
                tmp -> place = atoi( token );  
                memset( token, 0, strlen(token));

                tmp -> next = malloc( sizeof(seat));
                if (tmp -> next == NULL)     Error_("error in function : malloc.", 1); 
                last = tmp;
                tmp = tmp -> next;

            }   
            
            if (last != NULL) {
                last -> next = NULL;      
                free(tmp);
            } 
            return 0;
            
        }
    
    }

    return 1;

}