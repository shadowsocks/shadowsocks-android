 /*
 |----------------------------------------------------------------
 |             Copyright (c) 2002, venus.org.
 +----------------------------------------------------------------
 |
 |      IP relay source file
 |
 |      Vodoo Technologies ( Rachad 2003 -- ralao@venus.org )
 |                                              
 +----------------------------------------------------------------*/


/*---------------------------------------------------
 |     Include files below this line
 +---------------------------------------------------*/
#include "lib_ip_relay.h"
#include <stdio.h>

/*---------------------------------------------------
 |     Constant Definitions below this line
 +---------------------------------------------------*/
#define IR_ERROR_NONE           0                 /* normal exit code */
#define IR_ERROR_BAD_PARAMETERS 1                 /* bad command line parameters error code */
#define IR_ERROR_INIT_FAILED    2                 /* init failed error code */
#define IR_ERROR_INTERNAL_ERROR 4                 /* internal error code */
#define IR_READ_BUFFER_SIZE     1500              /* read buffer size in bytes */


/*---------------------------------------------------
 |     Type Definitions below this line
 +---------------------------------------------------*/

/* packet library context type */
typedef struct {
    unsigned short listen_port;
    char           *target_host;
    unsigned short target_port;
    unsigned long  timeout;
} IR_context;


/*---------------------------------------------------
 |     Global vars Definition below this line
 +---------------------------------------------------*/

/* global context */
static IR_context global_c = { 0, "localhost", 0, IPR_DEFAULT_TIMEOUT };


/*---------------------------------------------------
 |     Prototypes Definition below this line
 +---------------------------------------------------*/

/* ip_relay log msg function */
static IPR_log_function log_msg;


/*---------------------------------------------------
 |     External functions Definition below this line
 +---------------------------------------------------*/


/* converts IPR_log_level to strings */
static const char *level_to_str( IPR_log_level level )
{
    /* returns level string */
    switch ( level ) {
        case IPR_LL_INFO:
            return "INFO";
        case IPR_LL_WARNING:
            return "WARNING";
        case IPR_LL_ERROR:
            return "ERROR";
        default:
            return "";
    }

    /* returns */
    return "";
}


/* ip_relay log msg function */
static void log_msg(
    IPR_relay     *relay,       /**< relay logging message */
    IPR_log_level level,        /**< log level */
    const char    *msg,         /**< message to log */
    void          *client_data  /**< client data */
)
{
    /* prints log msg */
    fprintf( stderr, "ip_relay: %s - %s\n", level_to_str( level ), msg );
}



/* parses command line
 * returns 1 on success, 0 on failure.
 */
static int parse_command_line( int argc, char **argv )
{
    /* exits here if OS is not posix compliant! */
    if ( argc == 0 ) return 0;

    /* parses command line */
    if ( argc < 4 ) {
        fprintf( stderr, "usage:\n" );
        fprintf( stderr,
            "\tip_relay listen_port target_ip target_port [timeout]\n\n" );
        fprintf( stderr, "\tlisten_port: \n\t\tlocal port to listen on for udp/tcp connections.\n" );
        fprintf( stderr, "\ttarget_ip: \n\t\tip address to redirect connections to\n" );
        fprintf( stderr, "\ttarget_port: \n\t\tport number to redirect connections to\n" );
        fprintf( stderr, "\ttimeout: \n\t\toptional udp/tcp connection timeout in seconds, default = %d\n",
                 IPR_DEFAULT_TIMEOUT );
        exit( IR_ERROR_BAD_PARAMETERS );
    }
    global_c.listen_port = (unsigned short)atoi( argv[1] );
    global_c.target_host = argv[2];
    global_c.target_port = (unsigned short)atoi( argv[3] );
    if ( argc > 4 ) {
        global_c.timeout = (unsigned long)atoi( argv[4] );
    }

    /* returns */
    return 0;
}



/*
 * this is the main function of the IR generic server
 */
int main( int argc, char **argv )
{
    int            ret_val =0;
    int            n_fd_ready;
    IPR_relay      *relay;
    static fd_set  read_mask;  /* masks moved to BSS to avoid crouding the stack */
    static fd_set  write_mask; /* masks moved to BSS to avoid crouding the stack */
    static fd_set  error_mask; /* masks moved to BSS to avoid crouding the stack */
    struct timeval timeout;

    /* parses command line */
    parse_command_line( argc, argv );

    /* inits lib IP relay */
    if ( ipr_class_init() < 0 ) {
        fprintf( stderr, "ip_relay: ERROR - could not init lib_ip_relay!\n" );
        return IR_ERROR_INIT_FAILED;
    }

    /* creates relay */
    relay = ipr_relay_new( global_c.listen_port, global_c.target_host, global_c.target_port,
                           global_c.timeout, log_msg, &global_c );
    if ( !relay ) {
        fprintf( stderr, "ip_relay: ERROR - could not create on localhost:%d!\n",
                 global_c.listen_port );
        ipr_class_delete( );
        return IR_ERROR_INIT_FAILED;
    }

    /*  prints running trace */
    fprintf( stderr, "ip_relay: running on localhost:%d <-> %s:%d\n",
             global_c.listen_port, global_c.target_host, global_c.target_port ) ;

    /* do forever */
    for ( ; /* ever */ ; ) {

        /* resets fd masks */
        FD_ZERO( &read_mask );
        FD_ZERO( &write_mask );
        FD_ZERO( &error_mask );
        timeout.tv_sec  = IPR_DEFAULT_TIMEOUT;
        timeout.tv_usec = 0;

        /* sets relay fds  */
        if ( ipr_select_fds_set( relay, &read_mask, &write_mask, &error_mask, &timeout ) < 0 ) {
            fprintf( stderr, "ip_relay: ERROR - could not set relay fd to select masks!\n" );
            ret_val = IR_ERROR_INTERNAL_ERROR;
            break;
        }

        /* sleeps on select */
        n_fd_ready = select( FD_SETSIZE, &read_mask, &write_mask, &error_mask, &timeout );

        /* ignores select errors */
        if ( n_fd_ready < 0 ) {
            fprintf( stderr, "ip_relay: WARNING - select failed! ignoring...\n") ;
            continue;
        }

        /* checks relay fds */
        if ( ipr_select_fds_check( relay, &read_mask, &write_mask, &error_mask, n_fd_ready ) < 0 ) {
            fprintf( stderr, "ip_relay: ERROR - could not check relay fds!\n" );
            ret_val = IR_ERROR_INTERNAL_ERROR;
            break;
        }
    }

    /* deletes relay  */
    ipr_relay_delete( relay );

    /* deletes lib ip_relay class */
    ipr_class_delete( );

    /* returns */
    return ret_val;
}
