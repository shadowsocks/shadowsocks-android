#ifndef _CONFIG_H_
#define _CONFIG_H_

/* ONLY EDIT acconfig.h, NEVER config.h or config.h.in!
 * config.h MAY BE OVERWRITTEN BY make, config.h.in by autoheader! */

/* Define your Target here. Currently defined are TARGET_LINUX (any
 * architecture), TARGET_BSD (experimental; tested on FreeBSD, hopefully
 * works for other BSD variants) and TARGET_CYGWIN. */
#define TARGET TARGET_LINUX

/* change the #undef to #define if you do not want to compile with special
 * ISDN support for Linux. Note that the ISDN support will not compile ok on
 * unpatched kernerls earlier than 2.2.12 (if you did apply newer isdn patches,
 * it may work fine). This is not on by default because it will cause compile
 * problems on some systems */
#undef ISDN_SUPPORT

/* The following regulates the IP Protocol support. Supported types are IPv4
 * and IPv6 (aka IPng). You may enable either or both of these protocols.
 * Enabling in this context means that support for the respective protocol
 * will be in the binary. When running the binary, one of the protocols may
 * be activated via command line switches. Note that activating both IPv4 and
 * IPv6 is pointless (and will not work because two UDP and two TCP threads
 * will be started that concur for ports). Because of that, it is not allowed.
 * When pdnsd runs with IPv6 activated it should be able to service queries
 * from IPv6 as well as from IPv4 hosts, provided that you host is configured
 * properly.
 * For each of the protocols there are two options: ENABLE_IPV4 and ENABLE_IPV6
 * control whether support for the respective protocol is available in the
 * binary. DEFAULT_IPV4 selects which protocol is enabled on pdnsd
 * startup by default. 1 means IPv4, while 0 means IPv6. If support for
 * a protocol was included in the executable, you can specify command line
 * parameters to activate or deactivate that protocol (the options are -4 and
 * -6), but it makes more sense to use the run_ipv4=on/off option in the
 * configuration file.
 * Make your choice. Note that IPv6 support is experimental in pdnsd.
 * In normal operation, you will currently only need IPv4. */
#undef ENABLE_IPV4
#define DEFAULT_IPV4 1
#undef ENABLE_IPV6

/* In all pdnsd versions before 1.0.6, DNS queries were always done over
 * TCP. Now, you have the choice. You can control that behaviour using
 * the -m command line switch, and you can give a preset here. There
 * are 3 different modes:
 * UDP_ONLY: This is undoubtedly the fastest query method, because
 *       no TCP negotiation needs to be done.
 * TCP_ONLY: This is slower than uo, but generally more secure
 *       against DNS spoofing. Note that some name servers on the
 *       internet do not support TCP queries, notably dnscache.
 * TCP_UDP: TCP, then UDP. If the TCP query fails with a "connection refused"-
 *       error or times out, the query is retried using UDP.
 * UDP_TCP: UDP, then TCP. If the UDP reply is truncated (i.e. the tc flag is set),
 *       the query is retried using TCP. */
#define M_PRESET UDP_ONLY

/* In addition to choosing the presets, you may also completely disable
 * one of the protocols (TCP for preset UDP_ONLY and UDP for preset TCP_ONLY).
 * This saves some executable space. */
#undef NO_UDP_QUERIES
#undef NO_TCP_QUERIES

/* With the following option, you can disable the TCP server functionality
 * of pdnsd. Nearly no program does TCP queries, so you probably can do
 * this safely and save some executable space and one thread.
 * You also can turn off the TCP server at runtime with the --notcp option. */
#undef NO_TCP_SERVER

/* By undefining the following, you can disable the UDP source address
 * discovery code. This is not recommended, but you may need it when
 * running into compilation problems. */
#undef SRC_ADDR_DISC

/* NO_POLL specifies not to use poll(2), but select(2) instead. If you are
 * unsure about what this means, just leave this as it is.*/
#undef NO_POLL

/* Define this for "hard" RFC 2181 compliance: this RFC states that
 * implementations should discard answers whose RR sets have multiple
 * different time stamps. While correct answers are generated, incorrect
 * ones are normally tolerated and corrected. Full RFC compliance is
 * however only achieved by deactivating this behaviour and thus being
 * intolerant. */
#undef RFC2181_ME_HARDER

/* Define this to the device you want to use for getting random numbers.
 * Leave this undefined if you wand to use the standard C library random
 * function, which basically should be sufficient.
 * Linux and FreeBSD have two random number devices: /dev/random and
 * /dev/urandom. /dev/urandom might be less secure in some cases, but
 * should still be more than sufficient. The use of /dev/random is
 * discouraged, as reading from this device blocks when new random bits
 * need to be gathered. */
#undef RANDOM_DEVICE
#undef R_DEFAULT
#undef R_RANDOM
#undef R_ARC4RANDOM
/*#define RANDOM_DEVICE "/dev/urandom"*/

/* Designate which database manager to use for cacheing.
 * default: native; others: gdbm */
#define CACHE_DBM DBM_NATIVE

#define CACHEDIR "/var/cache/pdnsd"

#define TEMPDIR "/tmp";

/* This is for various debugging facilities that produce debug output and
 * double-check some values. You can enable debug messages with the -g option.
 * Normally, you can switch this off safely by setting the number after DEBUG
 * to 0. This will increase speed (although only marginally), save space
 * in the executable (only about 12kB) and some stack space per thread
 * (which may be significant if you have many threads running simultaneously).
 * However, it may be an aid when debugging config files.
 * The only defined debug levels by now are in the range 0 - 9.
 * Define this to 9 if you want hex dumps of all the queries and replies pdnsd
 * receives (you must also call pdnsd with -v9 to actually see the hex dumps).
 * When in doubt, leave it defined to 1. */
#define DEBUG 1

/* This defines the default verbosity of informational messages you will get.
   This has nothing to to with the debug option (-g), but may be set with -v
   option. 0 is for normal operation, up to 3 for debugging.
   Unlike the debug messages, these messages will also be written to the syslog.*/
#define VERBOSITY 0

/* Redefine this if you want another hash size.
 * The number of hash buckets is computed as power of two (1<<HASH_SZ);
 * so e.g. HASH_SZ set to 10 yields 1024 hash rows.
 * HASH_SZ may not be bigger than 32 (if you set it even close to that value,
 * you are nuts.) */
#define HASH_SZ 10

/* Set this to debug the hash tables. Turn this off normally, or you will get
 * flooded with diagnostic messages */
#undef DEBUG_HASH

/* Define if you have working C99 Variadic macro support */
#undef CPP_C99_VARIADIC_MACROS

/* Define as int if socklen_t typedef is missing */
#undef socklen_t

/* Lock the UDP socket before using it? */
#undef SOCKET_LOCKING

/* Default TCP timeout when receiving queries */
#define TCP_TIMEOUT 30

/* Allow subsequent TCP queries on one connection? */
#undef TCP_SUBSEQ

/* Default value for parallel query number */
#define PAR_QUERIES   2

/* Maximum number of IP addresses used per nameserver obtained from NS records. */
#define MAXNAMESERVIPS 3

/* These are the possible targets. Normally no need to touch these
 * definitions. */
#define TARGET_LINUX  0
#define TARGET_BSD    1
#define TARGET_CYGWIN 2

/* Assume the Native POSIX Thread Library instead of LinuxThreads ? */
#undef THREADLIB_NPTL

/* If we are using LinuxThreads, implement the fix needed for newer glibcs ? */
#undef THREADLIB_LINUXTHREADS2

/* The following is needed for using LinuxThreads. Better don't touch. */
#define _REENTRANT 1
#define _THREAD_SAFE 1

/* It appears the newer versions of gcc won't convert a pointer to char into
   a pointer to unsigned char and vice versa without complaining.
   By using casts these warning messages can be suppressed, but at the cost
   of losing some type safety.
   Define charp and ucharp to be empty if you are a developer and find type
   safety more important.
   Leave the definitions unchanged to avoid distracting warning messages. */
#define charp (char *)
#define ucharp (unsigned char *)


/* pdnsd version. DO NOT TOUCH THIS! It is replaced automatically by the
 * contents of ./version */
#define VERSION "@VERSION@"

#endif
