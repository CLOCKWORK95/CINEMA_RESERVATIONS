#include "header.h"
#include "support.c"


int                     socket_descriptor,       num_conns = 0;


//mutex for synchronized threads' access on files 'cinema' and 'cinema_prenotazioni'.
pthread_mutex_t         CINEMA_MUTEX = PTHREAD_MUTEX_INITIALIZER;

//mutex for exit synchronization.
pthread_mutex_t         EXIT_MUTEX = PTHREAD_MUTEX_INITIALIZER;


struct sockaddr_in      my_address,     their_address;
                                                        

//Linked-list for dynamic memory allocation of client-server connections.
typedef struct concurrent_connection_ {

    int                                     sock_descr;
    int                                     connection_number;
    pthread_t                               tid;
    struct concurrent_connection_           *next;
    struct concurrent_connection_           *before;

} concurrent_connection ;                                   concurrent_connection   *new_conn;           concurrent_connection   *tmp;



//Thread Local Storage.
__thread int    descr,       my_conn_num,       file_descriptor;    

__thread seat   *seats_vector,                  *reservation_seats;

__thread concurrent_connection                  *my_conn;



//Semaphore_array ID for main thread's secure exit.
int sem_id;

//Index used by the secure_exit signal handler to identify the current-exiting-thread.
int exit_index = 0;




/*  FUNCTIONS' DECLARATIONS. */
void * connection_handler ( void * attr);

void send_seats_view();

void reserve_and_confirm();

void delete_reservation();

void build_cinema();

void build_reservation_seats();

int load_reservation();

int get_cancellation_seats();



/*  SIGINT Handler */
void sigHandler( int signo ){

    //temporarily block SIGINT occurrences.
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, 0);

    int ret;            struct sembuf oper;

    oper.sem_num = 0;
    oper.sem_flg = 0;
    oper.sem_op = -num_conns;


    pthread_kill( ( new_conn -> tid ), SIGUSR1 );

    ret = semop( sem_id, &oper, 1 );
    if (ret == -1)      Error_("Error in function : semop.", 1);


    printf("\n\n All threads has completed their tasks and exited. Server exits.\n\n");
    fflush(stdout);

    exit( EXIT_SUCCESS ); 

}


/*  SIGUSR1 Handler */
void secureExit( int signo ) {

    sigset_t set;
    sigemptyset( &set );
    sigaddset( &set, SIGUSR1 );
    sigprocmask( SIG_BLOCK, &set, 0);

    struct sembuf oper;         int ret;


    //critical section for exit_index_increment.
    pthread_mutex_lock( &EXIT_MUTEX );

    exit_index ++; 

    if( exit_index < num_conns )        new_conn = new_conn -> next; 

    pthread_mutex_unlock( &EXIT_MUTEX );  

    oper.sem_num = 0;
    oper.sem_flg = 0;
    oper.sem_op  = 1;
    
    again:
    ret = semop( sem_id, &oper, 1);
    if (ret == -1 && errno == EINTR ) {
        goto again;
    }

    if( exit_index < num_conns )        pthread_kill( new_conn -> tid, SIGUSR1 );

    sigprocmask( SIG_UNBLOCK, &set, 0 );



}



/*  Main thread sets up a welcome socket and maps on a well-known port. 
    Accepts connections and delegates connection_handler threads serving 
    requests. */
int main (int argc, char** argv){

    int ret,    addrlen;

    signal( SIGINT, sigHandler );

    //initiate semaphore structure for secure exit of Server.
    sem_id = semget( IPC_PRIVATE, 1, 0660);
    if (sem_id == -1) Error_("Error in function : semget.", 1);

    ret = semctl( sem_id, 0, SETVAL, 0);
    if (ret == -1)      Error_("Error in function : semctl.", 1);

    //open a session on a socket.
    socket_descriptor = socket( AF_INET, SOCK_STREAM, 0);
    if (socket_descriptor == -1)        Error_("error in function: socket.", 1);


    //initiate the address of this server-socket, and then bind it to the opened session.
    memset( &my_address, 0, sizeof( my_address));
    my_address.sin_family = AF_INET;
    my_address.sin_port = htons(PORT);
    my_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    int opt = 1;

    if ( setsockopt( socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt) ) < 0 ) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    if( setsockopt( socket_descriptor, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(opt) ) < 0 ) {
           perror("setsockopt");
           exit(EXIT_FAILURE);
    }

    ret = bind( socket_descriptor, (struct sockaddr *) &my_address, sizeof(my_address) );
    if (ret != 0)      Error_("error in function: bind.", 1);

    //connection-oriented socket has to be put on listening to accept one or more connection requests.
    ret = listen( socket_descriptor, MAX_PENDING);
    if (ret == -1)      Error_("error in function: listen.", 1);

    addrlen = sizeof( their_address );

    printf("        *** CINEMA RESERVATIONS SERVER ***   ");
    printf("\n\n    listening to connection requests...\n");


    new_conn = malloc( sizeof( concurrent_connection ) );
    if (new_conn == NULL)        Error_("error in function: malloc.", 1);

    tmp = new_conn;


    //requests handler loop.
    while ( 1 ) {

    redo:
        ret = accept( socket_descriptor, (struct sockaddr *) &their_address, &addrlen );
        if (ret == -1)  {
            if ( errno == EINTR ) goto redo;
            else return -1;
        }


            {   //generate a new child thread to handle the new client-server connection (server side).

                num_conns ++;

                memset( tmp, 0, sizeof(concurrent_connection) );
                tmp -> sock_descr = ret;
                tmp -> connection_number = num_conns;

                tmp -> next = malloc( sizeof( concurrent_connection ) );
                if (tmp -> next == NULL)        Error_("error in function: malloc.", 1);

                ret = pthread_create( &( tmp -> tid ), NULL, connection_handler, (void *) tmp ); 
                if (ret == -1)      Error_("error in function: pthread_create.", 1);

                concurrent_connection *before = tmp;
                tmp = tmp -> next;
                tmp -> before = before;


            }


    }


    return EXIT_SUCCESS;
    
}



/*  Concurrent-thread handling a connection to client. */
void * connection_handler (void * attr) {

    int             ret,    request_code;

    struct sembuf   oper;

    // initiate a signal mask and block SIGINT.
    sigset_t set;
    sigemptyset( &set );
    sigaddset( &set, SIGINT );
    sigprocmask( SIG_BLOCK, &set, 0);

    sigemptyset( &set );
    sigaddset( &set, SIGUSR1);
    signal( SIGUSR1, secureExit );

    char buffer[MAX_LINE];

    //set the TLS.
    my_conn = (concurrent_connection *) attr;
    descr = (int)  my_conn -> sock_descr;
    my_conn_num = (int) my_conn -> connection_number;


    //print connection infos on screen.
    printf("\n\n\nThis is thread n° %ld of CINEMA RESERVATION SERVER.", pthread_self() );
    printf("\nManaging connection on socket n° %d .", descr);
    printf("\nConnection n° %d established : Ready to answer client requests...", my_conn_num );
    fflush(stdout);

    struct timeval timeout; 

    //set socket timeout
    timeout.tv_sec = (time_t) 10;

    if ( setsockopt (descr, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) < 0 )
        Error_("setsockopt failed\n", 1);


    do{

        ret = Readline( descr, buffer, MAX_LINE );
        if (ret == -1) {
            pthread_exit(NULL);
        }
        
        if (ret != 0){

            request_code = atoi( buffer);
            printf("\nrequest code : %d  -  ", request_code);
            fflush(stdout);

            switch(request_code) {

                case 0:
                        //UNCONNECT
                        num_conns --;
                        close( descr );
                        printf("Thread n°%ld exiting. Connection %d closed.", ( my_conn-> tid ), my_conn_num );
                        fflush(stdout);
                        pthread_exit( (void *) 0);

                case 1:
                        //SEND SEATS VIEW
                        sigprocmask(SIG_BLOCK, &set, 0);

                        send_seats_view();

                        if ( sigpending( &set ) == 0 ) {

                            if ( sigismember( &set, SIGUSR1 ) ) {

                                printf("thread n° %ld exits\n", pthread_self() );
                                fflush(stdout);

                                oper.sem_num = 0;
                                oper.sem_flg = 0;
                                oper.sem_op  = 1;
                                
                                again_1:
                                ret = semop( sem_id, &oper, 1);
                                if (ret == -1 && errno == EINTR ) {
                                    goto again_1;
                                }

                                pthread_mutex_lock( &EXIT_MUTEX );

                                exit_index ++; 

                                pthread_mutex_unlock( &EXIT_MUTEX );
                            

                                if ( exit_index < num_conns ) {
                                    new_conn = new_conn -> next;  
                                    pthread_kill( new_conn -> tid, SIGUSR1 ); 
                                }
                                
                                pthread_exit( (void*) -1 );

                            }
                        }

                        sigemptyset( &set );
                        sigaddset( &set, SIGUSR1 );
                        sigprocmask(SIG_UNBLOCK, &set, 0);

                        break;

                case 2:
                        //RESERVE AND CONFIRM
                        sigprocmask(SIG_BLOCK, &set, 0);

                        reserve_and_confirm();

                        if ( sigpending( &set ) == 0 ) {

                            if ( sigismember( &set, SIGUSR1 ) ) {

                                printf("thread n° %ld exits\n", pthread_self() );
                                fflush(stdout);

                                oper.sem_num = 0;
                                oper.sem_flg = 0;
                                oper.sem_op  = 1;
                                
                                again_2:
                                ret = semop( sem_id, &oper, 1);
                                if (ret == -1 && errno == EINTR ) {
                                    goto again_2;
                                }

                                pthread_mutex_lock( &EXIT_MUTEX );

                                exit_index ++; 

                                pthread_mutex_unlock( &EXIT_MUTEX );
                            

                                if ( exit_index < num_conns ) {
                                    new_conn = new_conn -> next;  
                                    pthread_kill( new_conn -> tid, SIGUSR1 ); 
                                }
                                
                                pthread_exit( (void*) -1 );

                            }
                        }

                        sigemptyset( &set );
                        sigaddset( &set, SIGUSR1 );
                        sigprocmask(SIG_UNBLOCK, &set, 0);

                        break;

                case 3:
                        //DELETE RESERVATION
                        sigprocmask(SIG_BLOCK, &set, 0);

                        delete_reservation();

                        if ( sigpending( &set ) == 0 ) {

                            if ( sigismember( &set, SIGUSR1 ) ) {

                                printf("thread n° %ld exits\n", pthread_self() );
                                fflush(stdout);

                                oper.sem_num = 0;
                                oper.sem_flg = 0;
                                oper.sem_op  = 1;
                                
                                again_3:
                                ret = semop( sem_id, &oper, 1);
                                if (ret == -1 && errno == EINTR ) {
                                    goto again_3;
                                }

                                pthread_mutex_lock( &EXIT_MUTEX );

                                exit_index ++; 

                                pthread_mutex_unlock( &EXIT_MUTEX );
                            

                                if ( exit_index < num_conns ) {
                                    new_conn = new_conn -> next;  
                                    pthread_kill( new_conn -> tid, SIGUSR1 ); 
                                }
                                
                                pthread_exit( (void*) -1 );

                            }
                        }

                        sigemptyset( &set );
                        sigaddset( &set, SIGUSR1 );
                        sigprocmask(SIG_UNBLOCK, &set, 0);

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

    //open a session on file "cinema", to serve client requests.

    f = fopen( "cinema", "r" );
    if (f == NULL)      Error_("error in function: fopen (send seats view).", 1);


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
    
    return;

}


void reserve_and_confirm() {

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

    char    *l = NULL,         *token;

    int     ret,               desc;
    
    size_t len;

    seat    *tmp = NULL,       *last = NULL;

    FILE    *f, *codes;

    //find the code within reservation file 
    codes = fopen( "cinema_prenotazioni", "r+" );
    if ( codes == NULL )       Error_("error in function: fopen (delete reservation).", 1);

    //ensure atomicity of transaction.
    pthread_mutex_lock( &CINEMA_MUTEX ); 

    f = fopen( "cinema" , "r+" );
    if (f == NULL)      Error_("error in function: fopen (delete reservation).", 1);

    build_cinema( f );

    int bytes = 0;          int codelen = 0;


    //get the reservation code from the client.
    char reservation_code[MAX_LINE];

    ret = Readline( descr, reservation_code, MAX_LINE );
    if (ret == -1)      Error_("error in function : Readline.", 1);

    printf("\n Buffer content : %s", reservation_code);       fflush(stdout);  

    ret = get_cancellation_seats( codes, reservation_code, &bytes, &codelen );

    if( ret != 0 )      goto end2;
    else                printf("reservation found!");

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
    fseek(codes, 0, SEEK_SET);
    fflush( codes );          fclose( codes );
    free( reservation_seats );
    free( seats_vector );
    printf("\n TRANSACTION COMPLETE. Reservation has been successfully deleted."); fflush(stdout);
    return;

    end2:
    pthread_mutex_unlock( &CINEMA_MUTEX );
    fseek(codes, 0, SEEK_SET);
    fflush( codes );          fclose( codes );
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


int get_cancellation_seats( FILE *codes, char * reservation_code, int *bytes, int *codelen ) {

    char    *l = NULL,     *token = NULL;   

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

                tmp -> next = malloc( sizeof(seat) );
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
        
        l = NULL; len = (size_t) 0;
    
    }

    return 1;

}


