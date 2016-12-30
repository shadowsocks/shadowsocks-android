/*
 |----------------------------------------------------------------
 |             Copyright (c) 2003, Voodoo Technologies.
 +----------------------------------------------------------------
 |
 |      Lib IP relay header file
 |
 |      Vodoo Technologies ( Rachad 2003 -- ralao@venus.org )
 |
 +----------------------------------------------------------------*/


#ifndef _LIB_IP_RELAY_H_
#define _LIB_IP_RELAY_H_


/*---------------------------------------------------
 |     Include files below this line
 +---------------------------------------------------*/
#define FD_SETSIZE 1024 /* sets maximum number of fds per process to 4096 */
#ifdef _WIN32
#include <Winsock2.h>
#else
#ifdef _ANDROID
#include <unistd.h>
#include <sys/un.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#endif /* _WIN32 */


/* adds C++ extern C def after include files if needed */
#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------
 |     Constant Definitions below this line
 +---------------------------------------------------*/
#define IPR_ANY_IP_ADDRESS      (-1)  /**< ipr any ip address */
#define IPR_DEFAULT_TIMEOUT      86400 /**< default connection timeout in seconds (1 day) */

/*---------------------------------------------------
 |     Macros Definitions below this line
 +---------------------------------------------------*/

/*---------------------------------------------------
 |     Type Definitions below this line
 +---------------------------------------------------*/

/** IPR relay object type */
typedef struct _IPR_relay IPR_relay;


/** IPR log level enum type */
typedef enum {
    IPR_LL_INFO,    /**< Info log level */
    IPR_LL_WARNING, /**< Warning log level */
    IPR_LL_ERROR    /**< Error log level */
} IPR_log_level;


/* IPR logging function */
typedef void (IPR_log_function)(
    IPR_relay     *relay,       /**< relay logging message */
    IPR_log_level level,        /**< log level */
    const char    *msg,         /**< message to log */
    void          *client_data  /**< client data */
);

/*---------------------------------------------------
 |     External vars Definition below this line
 +---------------------------------------------------*/

/*---------------------------------------------------
 |     External functions Definition below this line
 +---------------------------------------------------*/


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
extern int ipr_class_init( void );


/**
 * This function is used to free all the memory allocated by the ipr
 * library for its internal house keeping. It should be called when
 * the client is done using the ipr library.
 * returns 0 if the class has been successfully deleted and a negative
 * value in case of destruction failure.
 */
extern int ipr_class_delete( void );


/**
 * This function creates a new ip relay object.  The relay is automatically
 * started upon creation.
 * returns a pointer to the newly created IPR_relay object on success,
 * or NULL on failure.
 */
extern IPR_relay *ipr_relay_new(
    unsigned short   local_port,         /**< local port to relay */
    const char       *remote_hostname,   /**< remote hostname to relay traffic to */
    unsigned short   remote_port,        /**< remote port to relay traffic to */
    unsigned long    connection_timeout, /**< client connection timeout in seconds */
    IPR_log_function *log_function,      /**< optional user provided logging that will be used to log
                                              events */
    void             *lf_client_data     /**< client data to pass to logging function when it is
                                              invoked */
);


/**
 * This function stops and deletes a previously created IP relay object.
 * returns 0 on success or a strictly negative value on failure.
 */
extern int ipr_relay_delete(
    IPR_relay *relay  /**< ip relay object to delete */
);


/**
 * This function sets the file descriptors used by an IP relay to the
 * the user provided select fd_set masks.
 * returns 0 on success or a strictly negative valure on failure.
 */
extern int ipr_select_fds_set(
    IPR_relay      *relay,       /**< ip relay object */
    fd_set         *read_mask,   /**< user provided read fd_set mask */
    fd_set         *write_mask,  /**< user provided read fd_set mask */
    fd_set         *error_mask,  /**< user provided read fd_set mask */
    struct timeval *timeout      /**< user provided pointer to select timeout */
);


/**
 * This function checks the file descriptors used by an IP relay
 * against the the user provided select fd_set masks.
 * returns 0 on success or a strictly negative valure on failure.
 */
extern int ipr_select_fds_check(
    IPR_relay *relay,       /**< ip relay object */
    fd_set    *read_mask,   /**< user provided read fd_set mask */
    fd_set    *write_mask,  /**< user provided read fd_set mask */
    fd_set    *error_mask,  /**< user provided read fd_set mask */
    int       n_fd_ready    /**  number of file descriptors ready (returned
                                 by select) */
);

extern char *android_workdir;



/* adds C++ extern "C" def } if needed */
#ifdef __cplusplus
}
#endif


#endif /* _LIB_IP_RELAY_H_ */
