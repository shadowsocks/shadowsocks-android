#ifndef UUID_FC148CFA_5ECC_488E_8A62_CD39406C9F1E
#define UUID_FC148CFA_5ECC_488E_8A62_CD39406C9F1E

/* evutil_socket_t is macros in libevent-2.0, not typedef, libevent-1.4 is
 * still supported because of Ubuntu 10.04 LTS */
#ifndef evutil_socket_t
#   warning Using hardcoded value for evutil_socket_t as libevent headers do not define it.
#   define evutil_socket_t int
#endif

#endif // FC148CFA_5ECC_488E_8A62_CD39406C9F1E
