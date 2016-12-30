/*
 |----------------------------------------------------------------
 |             Copyright (c) 2003, Voodoo Technologies.
 +----------------------------------------------------------------
 |
 |      Lib IP relay source file
 |
 |      Vodoo Technologies ( Rachad 2003 -- ralao@venus.org )
 |
 +----------------------------------------------------------------*/


/*---------------------------------------------------
 |     Include files below this line
 +---------------------------------------------------*/
#include "lib_ip_relay.h"

#ifdef _ANDROID
#include "ancillary.h"
#endif

#ifndef _WIN32

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#endif /* !_WIN32 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>


/*---------------------------------------------------
 |     Constant Definitions below this line
 +---------------------------------------------------*/
#define IPR_ERROR_CODE           (-1)   /* IPR default error code */
#define IPR_MAGIC_COOKIE         (('I'<<24) | ('P'<<16) | ('R'<<8) | 'R')
#define IPR_READ_BUFFER_SIZE     1500   /* read buffer size in bytes */
#define IPR_LISTEN_QUEUE_SIZE    1024   /* default IP relay listen queue size */

/*---------------------------------------------------
 |     Macros Definitions below this line
 +---------------------------------------------------*/
#ifdef _WIN32

#define INLINE         _inline
#define ERROR_STRING() (WSAGetLastError())

#else

#define BOOL           int
#define FALSE          0
#define TRUE           (!FALSE)
#define SOCKET         int
#define INVALID_SOCKET (-1)
#define ERROR_STRING()  (strerror(errno))
#define closesocket(s) (close(s))
#ifdef __GNUC__
#define INLINE         inline
#else
#define INLINE
#endif /* __GNUC__ */

#endif /* _WIN32 */

/*---------------------------------------------------
 |     Type Definitions below this line
 +---------------------------------------------------*/

/* tcp/ip relay node */
typedef struct _IPR_tcp_client_node {
    struct _IPR_tcp_client_node *next;
    long                        client_ip;
    int                         client_port;
    SOCKET                      client_socket;
    SOCKET                      target_socket;
    time_t                      last_used_time;
} IPR_tcp_client_node;


/* udp/ip relay node */
typedef struct _IPR_udp_client_node {
    struct _IPR_udp_client_node *next;
    long                        client_ip;
    int                         client_port;
    SOCKET                      target_socket;
    time_t                      last_used_time;
} IPR_udp_client_node;


/* _IPR_relay structure type */
struct _IPR_relay {
    long                magic_cookie;  /* should always be IPR_MAGIC_COOKIE for valid relays */
    int                 listen_port;
    SOCKET              tcp_client_listen_socket;
    SOCKET              udp_client_listen_socket;
    long                target_ip;
    int                 target_port;
    time_t              timeout;
    IPR_tcp_client_node *tcp_client_list;
    IPR_udp_client_node *udp_client_list;
    IPR_log_function    *log_function;
    void                *lf_client_data;
};


/* lib ip relay context type */
typedef struct {
    BOOL initialized;  /* TRUE if initialized, FALSE otherwise */
    long ref_count;    /* initialization ref count */
} IPR_context;


/*---------------------------------------------------
 |     Global vars Definition below this line
 +---------------------------------------------------*/
static IPR_context global_c = { FALSE, 0, NULL };  /* IP relay context */
#ifdef _ANDROID
char *android_workdir = NULL;
#endif

/*---------------------------------------------------
 |     External functions Definition below this line
 +---------------------------------------------------*/

#ifdef _ANDROID
/*
   copy from shadowsocks-libev/src/android.c
   use system connect-send-recv-close instead of lwip's'
 */
int protect_socket(int fd)
{
    int sock;
    struct sockaddr_un addr;

    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        return -1;
    }

    // Set timeout to 1s
    struct timeval tv;
    tv.tv_sec  = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval));

    char path[257];
    sprintf(path, "%s/protect_path", android_workdir);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(sock);
        return -1;
    }

    if (ancil_send_fd(sock, fd)) {
        close(sock);
        return -1;
    }

    char ret = 0;

    if (recv(sock, &ret, 1, 0) == -1) {
        close(sock);
        return -1;
    }

    close(sock);
    return ret;
}
#endif


/* inits socket library */
static int init_socket_lib( void )
{
#ifdef _WIN32

    WORD    wVersionRequested;
    WSADATA wsaData;

    wVersionRequested = MAKEWORD( 2, 2 );
    if ( WSAStartup( wVersionRequested, &wsaData ) != 0 ) {
        return IPR_ERROR_CODE;
    }

#endif /* _WIN32 */

    return 0;
}


/* closes socket library */
static int close_socket_lib( void )
{
#ifdef _WIN32

    WSACleanup( );

#endif /* _WIN32 */

    return 0;
}




/**
 * This function is used to initialize the ipr library. It must be
 * called before any other function of the ipr library. It will take
 * care of allocating and initializing any data needed by the ipr
 * library for its internal housekeeping. To avoid memory leaks,
 * ipr_class_delete must be called when the client is done using the
 * ipr library.
 * returns 0 if the class has been successfully initialized and a
 * negative value in case of initialization failure.
 */
int ipr_class_init( void )
{
    /* ref counts initialization */
    global_c.ref_count++;

    /* exits here if class is already initialized */
    if ( global_c.initialized ) return 0;

    /* inits socket lib */
    if ( init_socket_lib( ) < 0 ) {
        global_c.initialized = FALSE;
        global_c.ref_count   = 0;
        return IPR_ERROR_CODE;
    }

    /* sets init attributes */
    global_c.initialized = TRUE;
    global_c.ref_count   = 1;

    /* returns */
    return 0;
}


/**
 * This function is used to free all the memory allocated by the ipr
 * library for its internal house keeping. It should be called when
 * the client is done using the ipr library.
 * returns 0 if the class has been successfully deleted and a negative
 * value in case of destruction failure.
 */
int ipr_class_delete( void )
{
    /* ref uncounts the deletion */
    global_c.ref_count--;

    /* exits here if class is still in use or is already deleted */
    if ( global_c.ref_count > 0 || !global_c.initialized ) return 0;

    /* closes socket lib */
    close_socket_lib( );

    /* resets init attributes */
    global_c.initialized = FALSE;
    global_c.ref_count   = 0;

    /* returns */
    return 0;
}


/* ip_relay log msg function */
static void log_msg( IPR_relay *relay, IPR_log_level level, const char *msg, ... )
{
    time_t    current_time;
    struct tm *local_time;
    va_list   arg_list;
    char      time_str[256];
    char      msg_extended[4*1024];
    char      msg_str[4*1024+256];

    /* returns here if relay has no log function */
    if ( !relay->log_function ) return;

    /* prints time stamp */
    time( &current_time );
    local_time = localtime( &current_time );
    strftime( time_str, 256, "%m/%d/%y@%H:%M:%S", local_time );
    va_start( arg_list, msg );
    vsprintf( msg_extended, msg, arg_list );
    va_end( arg_list );
    sprintf( msg_str, "%s - %s", time_str, msg_extended );
    relay->log_function( relay, level, msg_str, relay->lf_client_data );
}




/* just converts an ascii string to an IP address */
static long ascii_to_ip( const char *str )
{
    int  i, j;
    long ip;
    char ip_field[4];

    /* gets each ip field, converts it into an int and appends it to the ip */
    for ( ip = 0, i = 0; i < 4; i++ ) {
        /* gets current field */
        for ( j = 0; j < 3 && *str && *str != '.'; ip_field[j++] = *(str++) );
        ip_field[j] = '\0';

        /* skips '.' if needed */
        if ( *str == '.' ) str++;

        /* updates ip address */
        ip = ( ip << 8 ) + atoi( ip_field );
    }

    /* returns */
    return ip;
}


/* just creates the listen sockets */
static int create_listen_sockets( IPR_relay *relay )
{
    BOOL   reuse_address = TRUE;  /* reuse address option */
    struct sockaddr_in sock_addr;

    /* creates server sockets */
    relay->tcp_client_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    relay->udp_client_listen_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if ( relay->tcp_client_listen_socket == INVALID_SOCKET ||
         relay->udp_client_listen_socket == INVALID_SOCKET )
    {
        log_msg( relay, IPR_LL_ERROR, "could not create listen sockets!" );
        return IPR_ERROR_CODE;
    }
    #ifdef _ANDROID
    protect_socket(relay->tcp_client_listen_socket);
    protect_socket(relay->udp_client_listen_socket);
    #endif

    /* binds server sockets (and starts listening on tcp socket ) */
    memset( &sock_addr, 0, sizeof(sock_addr) );
    sock_addr.sin_family      = AF_INET;
    sock_addr.sin_addr.s_addr = htonl( INADDR_ANY );
    sock_addr.sin_port        = htons((unsigned short)relay->listen_port);
    setsockopt( relay->tcp_client_listen_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_address, sizeof(BOOL) );
    if ( bind( relay->tcp_client_listen_socket, (struct sockaddr *)&sock_addr, sizeof(sock_addr) ) < 0 ) {
        log_msg( relay, IPR_LL_WARNING, "could not create bind tcp listen socket!" );
    }
    if ( bind( relay->udp_client_listen_socket, (struct sockaddr *)&sock_addr, sizeof(sock_addr) ) < 0 ) {
        log_msg( relay, IPR_LL_WARNING, "could not create bind udp listen socket!" );
    }
    if ( listen( relay->tcp_client_listen_socket, IPR_LISTEN_QUEUE_SIZE ) < 0 ) {
        log_msg( relay, IPR_LL_WARNING, "could not create listen on tcp socket!" );
    }

    /* returns */
    return 0;
}



/**
 * This function creates a new ip relay object.  The relay is automatically
 * started upon creation.
 * returns a pointer to the newly created IPR_relay object on success,
 * or NULL on failure.
 */
IPR_relay *ipr_relay_new(
    unsigned short   local_port,         /**< local port to relay */
    const char       *remote_hostname,   /**< remote hostname to relay traffic to */
    unsigned short   remote_port,        /**< remote port to relay traffic to */
    unsigned long    conneciton_timeout, /**< client connection timeout in seconds */
    IPR_log_function *log_function,      /**< optional user provided logging that will be used to log
                                              events */
    void             *lf_client_data     /**< client data to pass to logging function when it is
                                              invoked */
)
{
    long           ip;
    IPR_relay      *relay;
    struct hostent *hostentry;

    /* exits here on garbage calls */
    if ( !global_c.initialized || !remote_hostname ) return NULL;

    /* allocates relay structure */
    relay = (IPR_relay *)malloc( sizeof( IPR_relay ) );
    if ( !relay ) return NULL;

    /* intis structure */
    relay->magic_cookie = IPR_MAGIC_COOKIE;
    relay->listen_port  = local_port;
    hostentry           = gethostbyname( remote_hostname );
    if ( hostentry ) {
        memcpy( &ip, hostentry->h_addr, hostentry->h_length );
        relay->target_ip = ntohl( ip );
    }
    else {
        relay->target_ip = ascii_to_ip( remote_hostname );
    }
    relay->target_port     = remote_port;
    relay->timeout         = conneciton_timeout;
    relay->tcp_client_list = NULL;
    relay->udp_client_list = NULL;
    relay->log_function    = log_function;
    relay->lf_client_data  = lf_client_data;

    /* creates local listener sockets */
    if ( create_listen_sockets( relay ) < 0 ) {
        relay->magic_cookie = 0;
        free( relay );
        return NULL;
    }

    /* returns */
    return relay;
}


/**
 * This function stops and deletes a previously created IP relay object.
 * returns 0 on success or a strictly negative value on failure.
 */
int ipr_relay_delete(
    IPR_relay *relay  /**< ip relay object to delete */
)
{
    IPR_tcp_client_node *tcp_client, *tmp_tcp_client;
    IPR_udp_client_node *udp_client, *tmp_udp_client;

    /* exits here on garbage calls */
    if ( !global_c.initialized || !relay || relay->magic_cookie != IPR_MAGIC_COOKIE ) {
        return IPR_ERROR_CODE;
    }

    /* deletes client nodes */
    for ( tcp_client = relay->tcp_client_list; tcp_client; /* inc in loop */ ) {
        tmp_tcp_client = tcp_client;
        tcp_client     = tcp_client->next;
        closesocket ( tmp_tcp_client->client_socket );
        closesocket ( tmp_tcp_client->target_socket );
        free( tmp_tcp_client );
    }
    for ( udp_client = relay->udp_client_list; udp_client; /* inc in loop */ ) {
        tmp_udp_client = udp_client;
        udp_client     = udp_client->next;
        closesocket ( tmp_udp_client->target_socket );
        free( tmp_udp_client );
    }

    /* deletes listners */
    closesocket( relay->udp_client_listen_socket );
    closesocket( relay->tcp_client_listen_socket );

    /* deletes relay */
    relay->magic_cookie = 0;
    free( relay );

    /* returns */
    return 0;
}


/**
 * This function sets the file descriptors used by an IP relay to the
 * the user provided select fd_set read_masks.
 * returns 0 on success or a strictly negative valure on failure.
 */
int ipr_select_fds_set(
    IPR_relay      *relay,       /**< ip relay object */
    fd_set         *read_mask,   /**< user provided read fd_set mask */
    fd_set         *write_mask,  /**< user provided read fd_set mask */
    fd_set         *error_mask,  /**< user provided read fd_set mask */
    struct timeval *timeout      /**< user provided pointer to select timeout */)
{
    time_t              tv_sec;
    time_t              oldest_last_used_time;
    IPR_tcp_client_node *tcp_client;
    IPR_udp_client_node *udp_client;

    /* exits here on garbage calls */
    if ( !global_c.initialized || !relay || relay->magic_cookie != IPR_MAGIC_COOKIE ||
         !read_mask || !timeout )
    {
        return IPR_ERROR_CODE;
    }

    /* adds listner sockets to read read_mask */
    FD_SET( relay->tcp_client_listen_socket, read_mask );
    FD_SET( relay->udp_client_listen_socket, read_mask );

    /* resets oldest_last_used_time */
    oldest_last_used_time = time( NULL );

    /* adds client node sockets to read read_mask */
    for ( tcp_client = relay->tcp_client_list; tcp_client; tcp_client = tcp_client->next ) {
        FD_SET( tcp_client->client_socket, read_mask );
        FD_SET( tcp_client->target_socket, read_mask );
        if ( tcp_client->last_used_time < oldest_last_used_time ) {
            oldest_last_used_time = tcp_client->last_used_time;
        }
    }
    for ( udp_client = relay->udp_client_list; udp_client; udp_client = udp_client->next ) {
        FD_SET( udp_client->target_socket, read_mask );
        if ( udp_client->last_used_time < oldest_last_used_time ) {
            oldest_last_used_time = udp_client->last_used_time;
        }
    }

    /* computes timeout and sets it only if provided timeout is longer */
    tv_sec = oldest_last_used_time + relay->timeout - time( NULL );
    if ( tv_sec < 0 ) tv_sec = 0;
    if ( tv_sec < timeout->tv_sec ) {
        timeout->tv_sec  = tv_sec;
        timeout->tv_usec = 0;
    }

    /* returns */
    return 0;
}


/* adds a tcp client node to the tcp_client_list */
static int add_tcp_client( IPR_relay *relay, long client_ip, int client_port,
                           SOCKET tcp_client_socket, SOCKET tcp_target_socket )
{
    IPR_tcp_client_node *node;

    node = malloc( sizeof( IPR_tcp_client_node ) );
    if ( !node ) {
        log_msg( relay, IPR_LL_WARNING, "could not allocate memory for tcp node!");
        return IPR_ERROR_CODE;
    }
    node->client_ip          = client_ip;
    node->client_port        = client_port;
    node->client_socket      = tcp_client_socket;
    node->target_socket      = tcp_target_socket;
    node->last_used_time     = time( NULL );
    node->next               = relay->tcp_client_list;
    relay->tcp_client_list = node;

    return 0;
}


/* adds a udp client node the udp client list */
static int add_udp_client( IPR_relay *relay, long client_ip, int client_port,
                           SOCKET udp_target_socket )
{
    IPR_udp_client_node *node;

    node = malloc( sizeof( IPR_udp_client_node ) );
    if ( !node ) {
        log_msg( relay, IPR_LL_WARNING, "could not allocate memory for udp node!");
            return IPR_ERROR_CODE;
    }
    node->client_ip          = client_ip;
    node->client_port        = client_port;
    node->target_socket      = udp_target_socket;
    node->last_used_time     = time( NULL );
    node->next               = relay->udp_client_list;
    relay->udp_client_list = node;

    return 0;
}


/* lookups a client from a udp_client_list */
static IPR_udp_client_node *lookup_udp_client( IPR_relay *relay, long client_ip,
                                              int client_port )
{
    IPR_udp_client_node *node;

    for ( node = relay->udp_client_list; node; node = node->next ) {
        if ( node->client_ip == client_ip && node->client_port == client_port ) {
            return node;
        }
    }

    return NULL;
}


/* just creates tcp target socket */
static SOCKET create_tcp_target_socket( IPR_relay *relay )
{
    SOCKET             target_socket;
    int                port = relay->target_port;
    long               ip   = relay->target_ip;
    struct sockaddr_in sock_addr;

   /* creates target sockets */
    target_socket = socket( AF_INET, SOCK_STREAM, 0 );
    if ( target_socket == INVALID_SOCKET ) {
        log_msg( relay, IPR_LL_WARNING, "could not create tcp target socket!") ;
        return INVALID_SOCKET;
    }
    #ifdef _ANDROID
    protect_socket(target_socket);
    #endif

    /* connects target sockets */
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family      = AF_INET;
    sock_addr.sin_addr.s_addr = htonl( relay->target_ip );
    sock_addr.sin_port        = htons( (unsigned short)relay->target_port );
    if ( connect( target_socket, (struct sockaddr *)&sock_addr, sizeof(sock_addr) ) < 0 ) {
        log_msg( relay, IPR_LL_WARNING, "could not connect tcp target socket to %d.%d.%d.%d:%d!",
                 (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff, port );
        closesocket( target_socket );
        return INVALID_SOCKET;
    }

    /* returns */
    return target_socket;
}


/* just creates udp target socket */
static SOCKET create_udp_target_socket( IPR_relay *relay )
{
    SOCKET             target_socket;
    int                port = relay->target_port;
    long               ip   = relay->target_ip;
    struct sockaddr_in sock_addr;

   /* creates target sockets */
    target_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if ( target_socket == INVALID_SOCKET ) {
        log_msg( relay, IPR_LL_WARNING, "could not create target udp socket!") ;
        return INVALID_SOCKET;
    }
    #ifdef _ANDROID
    protect_socket(target_socket);
    #endif

    /* connects target sockets */
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family      = AF_INET;
    sock_addr.sin_addr.s_addr = htonl( relay->target_ip );
    sock_addr.sin_port        = htons( (unsigned short)relay->target_port );
    if ( connect( target_socket, (struct sockaddr *)&sock_addr, sizeof(sock_addr) ) < 0 ) {
        log_msg( relay, IPR_LL_WARNING, "could not connect udp target socket to %d.%d.%d.%d:%d!",
                 (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff, port );
        closesocket( target_socket );
        return INVALID_SOCKET;
    }

    /* returns */
    return target_socket;
}


/* just clean up unused client nodes */
static int cleanup_unused_clients( IPR_relay *relay )
{
    int                   port;
    long                  ip;
    IPR_tcp_client_node   *tcp_client, *prev_tcp_client;
    IPR_udp_client_node   *udp_client, *prev_udp_client;

    /* cleanup tcp clients */
    for ( prev_tcp_client = NULL, tcp_client = relay->tcp_client_list;
          tcp_client; /* inc in loop */ )
    {
        if ( tcp_client->last_used_time + relay->timeout < time( NULL ) ) {
            ip   = tcp_client->client_ip;
            port = tcp_client->client_port;
            log_msg( relay, IPR_LL_INFO, "tcp client connection %d.%d.%d.%d:%d timeout! cleaning up...",
                     (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff, port );
            if ( prev_tcp_client ) prev_tcp_client->next = tcp_client->next;
            else relay->tcp_client_list = tcp_client->next;
            closesocket( tcp_client->client_socket );
            closesocket( tcp_client->target_socket );
            free( tcp_client );
            tcp_client = ( prev_tcp_client ) ? prev_tcp_client->next : relay->tcp_client_list;
        }
        else {
            prev_tcp_client = tcp_client;
            tcp_client      = tcp_client->next;
        }
    }

    /* cleanup udp clients */
    for ( prev_udp_client = NULL, udp_client = relay->udp_client_list;
          udp_client; /* inc in loop */ )
    {
        if ( udp_client->last_used_time + relay->timeout < time( NULL ) ) {
            ip   = udp_client->client_ip;
            port = udp_client->client_port;
            log_msg( relay, IPR_LL_INFO, "udp client connection %d.%d.%d.%d:%d timeout! cleaning up...",
                     (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff, port ) ;
            if ( prev_udp_client ) prev_udp_client->next = udp_client->next;
            else relay->udp_client_list = udp_client->next;
            closesocket( udp_client->target_socket );
            free( udp_client );
            udp_client = ( prev_udp_client ) ? prev_udp_client->next : relay->udp_client_list;
        }
        else {
            prev_udp_client = udp_client;
            udp_client      = udp_client->next;
        }
    }

    /* returns */
    return 0;
}



/* just goes through all tcp clients and forwards packet as needed */
static int forward_tcp_packets( IPR_relay *relay, fd_set *read_mask )
{
    int                  port;
    long                 ip;
    long                 read_size;
    IPR_tcp_client_node  *tcp_client, *prev_tcp_client;
    static unsigned char read_buffer[IPR_READ_BUFFER_SIZE];

    for ( prev_tcp_client = NULL, tcp_client = relay->tcp_client_list;
          tcp_client; /* inc in loop */ )
    {
        /* sends packet from client to target */
        if ( FD_ISSET( tcp_client->client_socket, read_mask ) ) {
            ip   = tcp_client->client_ip;
            port = tcp_client->client_port;
            read_size = recv( tcp_client->client_socket, read_buffer, IPR_READ_BUFFER_SIZE, 0 );
            if ( read_size <= 0  ) {
                if ( read_size < 0 ) {
                    log_msg( relay, IPR_LL_WARNING, "tcp_recv client %d.%d.%d.%d:%d failed (%d)! cleaning up...",
                             (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff, port, ERROR_STRING() );
                }
                else {
                    log_msg( relay, IPR_LL_INFO, "connection to tcp client %d.%d.%d.%d:%d closed! cleaning up...",
                             (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff, port ) ;
                }
                if ( prev_tcp_client ) prev_tcp_client->next = tcp_client->next;
                else relay->tcp_client_list = tcp_client->next;
                closesocket ( tcp_client->client_socket );
                closesocket ( tcp_client->target_socket );
                free( tcp_client );
                tcp_client = ( prev_tcp_client ) ? prev_tcp_client->next : relay->tcp_client_list;
                continue;
            }
            if ( send( tcp_client->target_socket, read_buffer, read_size, 0 ) < 0 )
            {
                log_msg( relay, IPR_LL_WARNING, "tcp_send target failed(%d)! ignoring...",
                         ERROR_STRING() ) ;
                continue;
            }
            tcp_client->last_used_time = time( NULL );
        }

        /* sends packet from target to client */
        if ( FD_ISSET( tcp_client->target_socket, read_mask ) ) {
            ip   = tcp_client->client_ip;
            port = tcp_client->client_port;
            read_size = recv( tcp_client->target_socket, read_buffer, IPR_READ_BUFFER_SIZE, 0 );
            if ( read_size <= 0 ) {
                if ( read_size < 0 ) {
                    log_msg( relay, IPR_LL_WARNING, "tcp_recv target failed (%d)! ignoring...",
                             ERROR_STRING() ) ;
                }
                else {
                    log_msg( relay, IPR_LL_INFO, "connection to tcp target closed! cleaning up %d.%d.%d.%d:%d...",
                             (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff, port );
                }
                if ( prev_tcp_client ) prev_tcp_client->next = tcp_client->next;
                else relay->tcp_client_list = tcp_client->next;
                closesocket ( tcp_client->client_socket );
                closesocket ( tcp_client->target_socket );
                free( tcp_client );
                tcp_client = ( prev_tcp_client ) ? prev_tcp_client->next : relay->tcp_client_list;
                continue;
            }
            if ( send( tcp_client->client_socket, read_buffer, read_size, 0 ) < 0 )
            {
                log_msg( relay, IPR_LL_WARNING, "tcp_send client %d.%d.%d.%d:%d failed (%d)! ignoring...",
                         (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff, port, ERROR_STRING() );
                continue;
            }
            tcp_client->last_used_time = time( NULL );
        }

        /* moves to next client */
        prev_tcp_client = tcp_client;
        tcp_client      = tcp_client->next;
    }

    /* returns */
    return 0;
}



/* just goes through all udp clients and forwards packet as needed */
static int forward_udp_packets( IPR_relay *relay, fd_set *read_mask )
{
    int                  sa_length;
    SOCKET               target_socket;
    int                  port;
    long                 ip;
    long                 read_size;
    IPR_udp_client_node  *udp_client;
    struct sockaddr_in   sock_addr;
    static unsigned char read_buffer[IPR_READ_BUFFER_SIZE];

    /* reads packet from udp listener */
    if ( FD_ISSET( relay->udp_client_listen_socket, read_mask ) ) {
        sa_length  = sizeof( struct sockaddr_in );
        memset( &sock_addr, 0, sa_length );
        read_size  = recvfrom( relay->udp_client_listen_socket, read_buffer, IPR_READ_BUFFER_SIZE, 0,
                               (struct sockaddr *)&sock_addr, &sa_length );
        if ( read_size < 0 ) {
            log_msg( relay, IPR_LL_WARNING, "udp_recvfrom failed! ignoring..." );
        }
        else {
            ip         = ntohl( sock_addr.sin_addr.s_addr );
            port       = ntohs( sock_addr.sin_port );
            udp_client = lookup_udp_client( relay, ip, port );
            if ( !udp_client ) {
                target_socket = create_udp_target_socket( relay );
                if ( target_socket == INVALID_SOCKET ) {
                    log_msg( relay, IPR_LL_WARNING, "rejecting new udp connection from %d.%d.%d.%d:%d! (failure)",
                            (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff, port ) ;
                }
                else {
                    add_udp_client( relay, ip, port, target_socket );
                    log_msg( relay, IPR_LL_INFO, "accepted new udp connection from %d.%d.%d.%d:%d.",
                             (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff, port );
                    if ( send( target_socket, read_buffer, read_size, 0 ) < 0 ) {
                        log_msg( relay, IPR_LL_WARNING, "udp_send target failed (%d)! ignoring...",
                                 ERROR_STRING() ) ;
                    }
                }
            }
            else {
                if ( send( udp_client->target_socket, read_buffer, read_size, 0 ) < 0 ) {
                    log_msg( relay, IPR_LL_WARNING, "udp_send target failed (%d)! ignoring...",
                             ERROR_STRING() );
                    udp_client->last_used_time = time( NULL );
                }
            }
        }
    }

    /* goes through all udp target sockets and forward packets as needed */
    for ( udp_client = relay->udp_client_list; udp_client; udp_client = udp_client->next ) {
        if ( FD_ISSET( udp_client->target_socket, read_mask ) ) {
            ip   = udp_client->client_ip;
            port = udp_client->client_port;
            read_size = recv( udp_client->target_socket, read_buffer, IPR_READ_BUFFER_SIZE, 0 );
            if ( read_size < 0 ) {
                log_msg( relay, IPR_LL_WARNING, "udp_recv from target failed (%d)! ignoring...",
                         ERROR_STRING() );
                continue;
            }
            sa_length  = sizeof( struct sockaddr_in );
            memset( &sock_addr, 0, sa_length );
            sock_addr.sin_family      = AF_INET;
            sock_addr.sin_addr.s_addr = htonl( ip );
            sock_addr.sin_port        = htons( (unsigned short)port );
            if ( sendto( relay->udp_client_listen_socket, read_buffer, read_size, 0,
                (struct sockaddr *)&sock_addr, sizeof(struct sockaddr_in) ) < 0 )
            {
                log_msg( relay, IPR_LL_WARNING, "udp_sendto %d.%d.%d.%d:%d failed (%d)! ignoring...",
                         (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff, port,
                         ERROR_STRING() );
                continue;
            }
            udp_client->last_used_time = time( NULL );
        }
    }

    /* returns */
    return 0;
}



/**
 * This function checks the file descriptors used by an IP relay
 * against the the user provided select fd_set read_masks.
 * returns 0 on success or a strictly negative valure on failure.
 */
int ipr_select_fds_check(
    IPR_relay *relay,       /**< ip relay object */
    fd_set    *read_mask,   /**< user provided read fd_set mask */
    fd_set    *write_mask,  /**< user provided read fd_set mask */
    fd_set    *error_mask,  /**< user provided read fd_set mask */
    int       n_fd_ready    /**  number of file descriptors ready (returned
                                 by select) */

)
{
    SOCKET               client_socket;
    SOCKET               target_socket;
    int                  sa_length;
    int                  port;
    long                 ip;
    struct sockaddr_in   sock_addr;

    /* exits here on garbage calls */
    if ( !global_c.initialized || !relay || relay->magic_cookie != IPR_MAGIC_COOKIE ||
         !read_mask )
    {
        return IPR_ERROR_CODE;
    }

    /* catches select timeout -- gets read of unused clients */
    if ( n_fd_ready == 0  ) {
        cleanup_unused_clients( relay );
        return 0;
    }

    /* accepts TCP connections */
    if ( FD_ISSET( relay->tcp_client_listen_socket, read_mask ) ) {
        sa_length  = sizeof( struct sockaddr_in );
        memset( &sock_addr, 0, sa_length );
        client_socket = accept( relay->tcp_client_listen_socket, (struct sockaddr *)&sock_addr, &sa_length );
        if ( client_socket == INVALID_SOCKET ) {
            log_msg( relay, IPR_LL_WARNING, "tcp_accept failed! ignoring...") ;
            return 0;
        }
        ip            = ntohl( sock_addr.sin_addr.s_addr );
        port          = ntohs( sock_addr.sin_port );
        target_socket = create_tcp_target_socket( relay );
        if ( target_socket == INVALID_SOCKET ) {
            log_msg( relay, IPR_LL_WARNING, "rejecting new tcp connection from %d.%d.%d.%d:%d! (failure)",
                        (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff, port ) ;
            closesocket ( client_socket );
            return 0;
        }
        add_tcp_client( relay, ip, port, client_socket, target_socket );
        log_msg( relay, IPR_LL_INFO, "accepted new tcp connection from %d.%d.%d.%d:%d!",
                    (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff, port );
    }

    /* forwards TCP packets  */
    forward_tcp_packets( relay, read_mask );

    /* forwards UDP packets to target */
    forward_udp_packets( relay, read_mask );

    /* returns */
    return 0;
}
