/* Copyright 2011 Nick Mathewson, George Kadianakis
   See LICENSE for other credits and copying information
*/

/**
 * \file managed.c
 * \headerfile managed.h
 * \brief Implements the 'managed proxy' mode of obfsproxy.
 *
 * \details This is an implementation of managed proxies according to
 * the 180-pluggable-transport.txt specification. The entry point is
 * launch_managed_proxy() where a managed_proxy_t instance is created
 * to represent our managed proxy. We read and validate the
 * environment variables that tor should have prepared for us, launch
 * the appropriate listeners, and finally kickstart libevent to start
 * accepting connections.
 **/

#include "util.h"

#define MANAGED_PRIVATE
#include "managed.h"
#include "network.h"
#include "protocol.h"
#include "main.h"

#include "container.h"

#include <event2/event.h>
#include <event2/listener.h>

#ifndef _WIN32
#include <arpa/inet.h> /* for inet_ntoa() */
#endif

/** Holds all the parameters provided by tor through environment
    variables. */
typedef struct managed_env_vars_t {
  char *state_loc; /* where we should store our state */
  char *conf_proto_version; /* managed proxy configuration protocols tor supports */
  char *transports; /* pluggable transports tor wants us to launch */

  /* server-only */
  char *extended_port; /* extended OR port tor wants us to use */
  char *or_port; /* ORPort that tor uses */
  char *bindaddrs; /* address to listen for client proxy connections */
} managed_env_vars_t;

/** Holds data for this managed proxy. */
typedef struct managed_proxy_t {
  unsigned int is_server : 1; /* whether we are a server or a client */

  managed_env_vars_t vars; /* environment variables */

  smartlist_t *configs; /* configs created */
} managed_proxy_t;

/* Holds the status of the environment variables parsing, so that we
   can report a granular error if something fails. */
enum env_parsing_status {
  ST_ENV_SMOOTH,
  ST_ENV_FAIL_STATE_LOC,
  ST_ENV_FAIL_CONF_PROTO_VER,
  ST_ENV_FAIL_CLIENT_TRANSPORTS,
  ST_ENV_FAIL_EXTENDED_PORT,
  ST_ENV_FAIL_ORPORT,
  ST_ENV_FAIL_BINDADDR,
  ST_ENV_FAIL_SERVER_TRANSPORTS,
};

/* Holds the status of a listener launch. */
enum launch_status {
  ST_LAUNCH_SMOOTH,
  ST_LAUNCH_FAIL_SETUP,
  ST_LAUNCH_FAIL_LSN,
};

/** Managed proxy protocol strings */
#define PROTO_ENV_ERROR "ENV-ERROR"
#define PROTO_NEG_SUCCESS "VERSION"
#define PROTO_NEG_FAIL "VERSION-ERROR no-version\n"
#define PROTO_CMETHOD "CMETHOD"
#define PROTO_SMETHOD "SMETHOD"
#define PROTO_CMETHOD_ERROR "CMETHOD-ERROR"
#define PROTO_SMETHOD_ERROR "SMETHOD-ERROR"
#define PROTO_CMETHODS_DONE "CMETHODS DONE\n"
#define PROTO_SMETHODS_DONE "SMETHODS DONE\n"

const char *supported_conf_protocols[] = { "1" };
const size_t n_supported_conf_protocols =
  sizeof(supported_conf_protocols)/sizeof(supported_conf_protocols[0]);

/**
   Make sure that we got *all* the necessary environment variables off tor.
*/
static inline void
assert_proxy_env(managed_proxy_t *proxy)
{
  obfs_assert(proxy);

  obfs_assert(proxy->vars.state_loc);
  obfs_assert(proxy->vars.conf_proto_version);
  obfs_assert(proxy->vars.transports);
  if (proxy->is_server) {
    obfs_assert(proxy->vars.extended_port);
    obfs_assert(proxy->vars.or_port);
    obfs_assert(proxy->vars.bindaddrs);
  } else {
    obfs_assert(!proxy->vars.extended_port);
    obfs_assert(!proxy->vars.or_port);
    obfs_assert(!proxy->vars.bindaddrs);
  }
}

/** Log the environment variables that tor prepared for <b>proxy</b>. */
static inline void
log_proxy_env(managed_proxy_t *proxy)
{
  obfs_assert(proxy);

  log_debug("Proxy environment:\n"
            "state_loc: '%s'\nconf_proto_version: '%s'\ntransports: '%s'",
            proxy->vars.state_loc,
            proxy->vars.conf_proto_version,
            proxy->vars.transports);

  if (proxy->is_server) {
    log_debug("Proxy environment (cont):\n"
              "extended_port: '%s'\nor_port: '%s'\nbindaddrs: '%s'",
              proxy->vars.extended_port,
              proxy->vars.or_port,
              proxy->vars.bindaddrs);
  }
}

/**
   This function is used for obfsproxy to communicate with tor.
   Practically a wrapper of printf, it prints 'format' and the
   fflushes stdout so that the output is always immediately available
   to tor.
*/
static void
print_protocol_line(const char *format, ...)
{
  va_list ap, ap2;
  va_start(ap,format);
#ifdef HAVE_VA_COPY
  va_copy(ap2, ap);
#else
  memcpy(&ap2, &ap, sizeof(ap));
#endif

  vprintf(format, ap);
  fflush(stdout);

  /* log the protocol message -- unless safe_logging is on, in which case
   * we censor it because the METHOD line contains the bridge address.
   */
  if (!safe_logging) {
    log_debug("We sent:");
    log_debug_raw(format, ap2);
  }

  va_end(ap);
  va_end(ap2);
}

/**
   Return a string containing the error we encountered while parsing
   the environment variables, according to 'status'.
*/
static inline const char *
get_env_parsing_error_message(enum env_parsing_status status)
{
  switch(status) {
  case ST_ENV_SMOOTH: return "no error";
  case ST_ENV_FAIL_STATE_LOC: return "failed on TOR_PT_STATE_LOCATION";
  case ST_ENV_FAIL_CONF_PROTO_VER: return "failed on TOR_PT_MANAGED_TRANSPORT_VER";
  case ST_ENV_FAIL_CLIENT_TRANSPORTS: return "failed on TOR_PT_CLIENT_TRANSPORTS";
  case ST_ENV_FAIL_EXTENDED_PORT: return "failed on TOR_PT_EXTENDED_SERVER_PORT";
  case ST_ENV_FAIL_ORPORT: return "failed on TOR_PT_ORPORT";
  case ST_ENV_FAIL_BINDADDR: return "failed on TOR_PT_SERVER_BINDADDR";
  case ST_ENV_FAIL_SERVER_TRANSPORTS: return "failed on TOR_PT_SERVER_TRANSPORTS";
  default:
    obfs_assert(0); return "UNKNOWN";
  }
}

/**
   Given:
   * comma-separated list of "<transport>-<bindaddr>" strings in 'all_bindaddrs'.
   * comma-separated list of "<transport>" strings in 'all_transports'.

   Return:
   * 0, if
   - all <transport> strings in 'all_bindaddrs' match with <transport>
     strings in 'all_transports' (order matters).
   AND
   - if all <bindaddr> strings in 'all_bindaddrs' are valid addrports.
   * -1, otherwise.
*/
int
validate_bindaddrs(const char *all_bindaddrs, const char *all_transports)
{
  int ret,i,n_bindaddrs,n_transports;
  struct evutil_addrinfo *bindaddr_test;
  char *bindaddr = NULL;
  char *transport = NULL;

  /* a list of "<proto>-<bindaddr>" strings */
  smartlist_t *bindaddrs = smartlist_create();
  /* a list holding (<proto>, <bindaddr>) for each 'bindaddrs' entry */
  smartlist_t *split_bindaddr = NULL;
  /* a list of "<proto>" strings */
  smartlist_t *transports = smartlist_create();

  smartlist_split_string(bindaddrs, all_bindaddrs, ",",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);
  smartlist_split_string(transports, all_transports, ",",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);

  n_bindaddrs = smartlist_len(bindaddrs);
  n_transports = smartlist_len(transports);

  if (n_bindaddrs != n_transports)
    goto err;

  for (i=0;i<n_bindaddrs;i++) {
    bindaddr = smartlist_get(bindaddrs, i);
    transport = smartlist_get(transports, i);

    split_bindaddr = smartlist_create();
    smartlist_split_string(split_bindaddr, bindaddr, "-",
                           SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);

    /* (<proto>, <bindaddr>) */
    if (smartlist_len(split_bindaddr) != 2)
      goto err;

    /* "all <transport> strings in 'all_bindaddrs' match with <transport>
       strings in 'all_transports' (order matters)." */
    if (strcmp(smartlist_get(split_bindaddr,0), transport))
      goto err;

    /* "if all <bindaddr> strings in 'all_bindaddrs' are valid addrports." */
    bindaddr_test = resolve_address_port(smartlist_get(split_bindaddr, 1),
                                         1, 1, NULL);
    if (!bindaddr_test)
      goto err;

    evutil_freeaddrinfo(bindaddr_test);
    SMARTLIST_FOREACH(split_bindaddr, char *, cp, free(cp));
    smartlist_free(split_bindaddr);
    split_bindaddr = NULL;
  }

  ret = 0;
  goto done;

 err:
  ret = -1;

 done:
  SMARTLIST_FOREACH(bindaddrs, char *, cp, free(cp));
  smartlist_free(bindaddrs);
  SMARTLIST_FOREACH(transports, char *, cp, free(cp));
  smartlist_free(transports);
  if (split_bindaddr) {
    SMARTLIST_FOREACH(split_bindaddr, char *, cp, free(cp));
    smartlist_free(split_bindaddr);
  }

  return ret;
}

/**
   Validates the environment variables that we got off tor.
*/
static int
validate_environment(managed_proxy_t *proxy)
{
  enum env_parsing_status status;

  assert_proxy_env(proxy);

  if (log_do_debug()) log_proxy_env(proxy);

  if (proxy->is_server) {
    if (validate_bindaddrs(proxy->vars.bindaddrs, proxy->vars.transports) < 0) {
      status = ST_ENV_FAIL_BINDADDR;
      goto err;
    }
  }

  return 0;

 err:
  print_protocol_line("%s %s\n", PROTO_ENV_ERROR,
                      get_env_parsing_error_message(status));

  return -1;
}

/**
   Free memory allocated by 'proxy'.
*/
static void
managed_proxy_free(managed_proxy_t *proxy)
{
  free(proxy->vars.state_loc);
  free(proxy->vars.conf_proto_version);
  free(proxy->vars.transports);
  free(proxy->vars.extended_port);
  free(proxy->vars.or_port);
  free(proxy->vars.bindaddrs);

  SMARTLIST_FOREACH(proxy->configs, config_t *, cfg, config_free(cfg));
  smartlist_free(proxy->configs);

  free(proxy);
}

/**
   Print a CMETHOD/SMETHOD line saying that we launched 'transport'
   with 'cfg' under 'proxy'.
*/
static void
print_method_line(const char *transport, const managed_proxy_t *proxy,
                  config_t *cfg)
{
  struct evconnlistener *listener = get_evconnlistener_by_config(cfg);
  obfs_assert(listener);

  struct sockaddr_in saddr;
  memset(&saddr,0,sizeof(struct sockaddr_in));
  socklen_t slen = sizeof(saddr);

  obfs_assert(!(getsockname(evconnlistener_get_fd(listener),
                            (struct sockaddr *)&saddr, &slen) < 0));

  if (proxy->is_server) {
    print_protocol_line("%s %s %s:%hu\n",
                        PROTO_SMETHOD,
                        transport,
                        inet_ntoa(saddr.sin_addr),
                        ntohs(saddr.sin_port));
  } else {
    print_protocol_line("%s %s %s %s:%hu\n",
                        PROTO_CMETHOD,
                        transport,
                        "socks5",
                        inet_ntoa(saddr.sin_addr),
                        ntohs(saddr.sin_port));

  }
}

/**
   Print the '{S,C}METHOD DONE' message.
*/
static inline void
print_method_done_line(const managed_proxy_t *proxy)
{
  print_protocol_line("%s", proxy->is_server ?
                      PROTO_SMETHODS_DONE : PROTO_CMETHODS_DONE);
}

/**
   Return true if 'name' matches the name of a supported managed proxy
   configuration protocol.
*/
static inline int
is_supported_conf_protocol(const char *name) {
  int f;
  for (f=0;f<n_supported_conf_protocols;f++) {
    if (!strcmp(name,supported_conf_protocols[f]))
      return 1;
  }
  return 0;
}

/**
   Finish the configuration protocol version negotiation by printing
   whether we support any of the suggested configuration protocols in
   stdout, according to the 180 spec.

   Return:
   * 0: if we actually found and selected a protocol.
   * -1: if we couldn't find a common supported protocol or if we
     couldn't even parse tor's supported protocol list.

   XXX: in the future we should return the protocol version we
   selected. let's keep it simple for now since we have just one
   protocol version.
*/
static int
conf_proto_version_negotiation(const managed_proxy_t *proxy)
{
  int r=-1;

  smartlist_t *versions = smartlist_create();
  smartlist_split_string(versions, proxy->vars.conf_proto_version, ",",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);

  SMARTLIST_FOREACH_BEGIN(versions, char *, version) {
    if (is_supported_conf_protocol(version)) {
      print_protocol_line("%s %s\n", PROTO_NEG_SUCCESS, version);
      r=0;
      goto done;
    }
  } SMARTLIST_FOREACH_END(version);

  /* we get here if we couldn't find a supported protocol */
  print_protocol_line("%s", PROTO_NEG_FAIL);

 done:
  SMARTLIST_FOREACH(versions, char *, cp, free(cp));
  smartlist_free(versions);

  return r;
}


/**
   Read the environment variables defined in the
   180-pluggable-transport.txt spec and fill 'proxy' accordingly.

   Return 0 if all necessary environment variables were set properly.
   Return -1 otherwise.
*/
static int
handle_environment(managed_proxy_t *proxy)
{
  char *tmp;
  enum env_parsing_status status=ST_ENV_SMOOTH;

  tmp = getenv("TOR_PT_STATE_LOCATION");
  if (!tmp) {
    status = ST_ENV_FAIL_STATE_LOC;
    goto err;
  }
  proxy->vars.state_loc = xstrdup(tmp);

  tmp = getenv("TOR_PT_MANAGED_TRANSPORT_VER");
  if (!tmp) {
    status = ST_ENV_FAIL_CONF_PROTO_VER;
    goto err;
  }
  proxy->vars.conf_proto_version = xstrdup(tmp);

  tmp = getenv("TOR_PT_CLIENT_TRANSPORTS");
  if (tmp) {
    proxy->vars.transports = xstrdup(tmp);
    proxy->is_server = 0;
  } else {
    proxy->is_server = 1;
  }

  if (proxy->is_server) {
    tmp = getenv("TOR_PT_EXTENDED_SERVER_PORT");
    if (!tmp) {
      proxy->vars.extended_port = xstrdup("");
    } else {
      proxy->vars.extended_port = xstrdup(tmp);
    }

    tmp = getenv("TOR_PT_ORPORT");
    if (!tmp) {
      status = ST_ENV_FAIL_ORPORT;
      goto err;
    }
    proxy->vars.or_port = xstrdup(tmp);

    tmp = getenv("TOR_PT_SERVER_BINDADDR");
    if (tmp) {
      proxy->vars.bindaddrs = xstrdup(tmp);
    } else {
      status = ST_ENV_FAIL_BINDADDR;
      goto err;
    }

    tmp = getenv("TOR_PT_SERVER_TRANSPORTS");
    if (!tmp) {
      status = ST_ENV_FAIL_SERVER_TRANSPORTS;
      goto err;
    }
    proxy->vars.transports = xstrdup(tmp);
  }

  return 0;

 err:
  print_protocol_line("%s %s\n", PROTO_ENV_ERROR,
                      get_env_parsing_error_message(status));

  return -1;
}

/**
    Return a string containing the error we encountered while
    launching a listener, according to 'status'.
*/
static inline const char *
get_launch_error_message(enum launch_status status)
{
  switch (status) {
  case ST_LAUNCH_SMOOTH:    return "no error";
  case ST_LAUNCH_FAIL_SETUP:   return "could not setup protocol";
  case ST_LAUNCH_FAIL_LSN:   return "could not launch listener";
  default:
    obfs_assert(0); return "UNKNOWN";
  }
}

/**
   Given the name of a protocol we tried to launch in 'protocol'
   the managed proxy parameters in 'proxy' and a listener launch
   status in 'status', print a line of the form:
       CMETHOD-ERROR <methodname> "<errormessage>"
   to signify a listener launching failure.
*/
static inline void
print_method_error_line(const char *protocol,
                        const managed_proxy_t *proxy,
                        enum launch_status status)
{
  print_protocol_line("%s %s %s\n",
                      proxy->is_server ?
                      PROTO_SMETHOD_ERROR : PROTO_CMETHOD_ERROR,
                      protocol,
                      get_launch_error_message(status));
}

/**
    Given a smartlist of listener configs; launch them all and print
    method lines as appropriate.  Return 0 if at least a listener was
    spawned, -1 otherwise.
*/
static int
open_listeners_managed(const managed_proxy_t *proxy)
{
  int ret=-1;
  const char *transport;

  /* Open listeners for each configuration. */
  SMARTLIST_FOREACH_BEGIN(proxy->configs, config_t *, cfg) {
    transport = get_transport_name_from_config(cfg);

    if (open_listeners(get_event_base(), cfg)) {
      ret=0; /* success! launched at least one listener. */
      print_method_line(transport, proxy, cfg);
    } else { /* fail. print a method error line. */
      print_method_error_line(transport, proxy, ST_LAUNCH_FAIL_LSN);
    }
  } SMARTLIST_FOREACH_END(cfg);

  return ret;
}

/**
   Launch all server listeners that tor wants us to launch.
*/
static int
launch_server_listeners(const managed_proxy_t *proxy)
{
  int ret=-1;
  int i, n_transports;
  const char *transport = NULL;
  const char *bindaddr_temp = NULL;
  const char *bindaddr = NULL;
  config_t *cfg = NULL;

  /* list of transports */
  smartlist_t *transports = smartlist_create();
  /* a list of "<transport>-<bindaddr>" strings */
  smartlist_t *bindaddrs = smartlist_create();

  /* split the comma-separated transports/bindaddrs to their smartlists */
  smartlist_split_string(transports, proxy->vars.transports, ",",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);
  smartlist_split_string(bindaddrs, proxy->vars.bindaddrs, ",",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);

  n_transports = smartlist_len(transports);

  /* Iterate transports; match them with their bindaddr; create their
     config_t. */
  for (i=0;i<n_transports;i++) {
    transport = smartlist_get(transports, i);
    bindaddr_temp = smartlist_get(bindaddrs, i);

    obfs_assert(strlen(bindaddr_temp) > strlen(transport)+1);

    bindaddr = bindaddr_temp+strlen(transport)+1; /* +1 for the dash */

    cfg = config_create_managed(proxy->is_server,
                                transport, bindaddr, proxy->vars.or_port);
    if (cfg) /* if a config was created; put it in the config smartlist */
      smartlist_add(proxy->configs, cfg);
    else /* otherwise, spit a method error line */
      print_method_error_line(transport, proxy, ST_LAUNCH_FAIL_SETUP);
  }

  /* open listeners */
  ret = open_listeners_managed(proxy);

  SMARTLIST_FOREACH(bindaddrs, char *, cp, free(cp));
  smartlist_free(bindaddrs);
  SMARTLIST_FOREACH(transports, char *, cp, free(cp));
  smartlist_free(transports);

  print_method_done_line(proxy); /* print {C,S}METHODS DONE */

  return ret;
}

/**
   Launch all client listeners that tor wants us to launch.
*/
static int
launch_client_listeners(const managed_proxy_t *proxy)
{
  int ret=-1;
  config_t *cfg = NULL;

  /* list of transports */
  smartlist_t *transports = smartlist_create();

  smartlist_split_string(transports, proxy->vars.transports, ",",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, -1);

  SMARTLIST_FOREACH_BEGIN(transports, char *, transport) {
    /* clients should find their own listen port */
    cfg = config_create_managed(proxy->is_server,
                                transport, "127.0.0.1:0", proxy->vars.or_port);
    if (cfg) /* if config was created; put it in the config smartlist */
      smartlist_add(proxy->configs, cfg);
    else
        print_method_error_line(transport, proxy, ST_LAUNCH_FAIL_SETUP);
  } SMARTLIST_FOREACH_END(transport);

  /* open listeners */
  ret = open_listeners_managed(proxy);

  SMARTLIST_FOREACH(transports, char *, cp, free(cp));
  smartlist_free(transports);

  print_method_done_line(proxy); /* print {C,S}METHODS DONE */

  return ret;
}

/**
   Launch all listeners that tor wants us to launch.
   Return 0 if at least a listener was launched, -1 otherwise.
*/
static inline int
launch_listeners(const managed_proxy_t *proxy)
{
  if (proxy->is_server)
    return launch_server_listeners(proxy);
  else
    return launch_client_listeners(proxy);
}

/**
   This function fires up the managed proxy.
   It first reads the environment that tor should have prepared, and then
   launches the appropriate listeners.

   Returns 0 if we managed to launch at least one proxy,
   returns -1 if something went wrong or we didn't launch any proxies.
*/
int
launch_managed_proxy(void)
{
  int r=-1;
  managed_proxy_t *proxy = xzalloc(sizeof(managed_proxy_t));
  proxy->configs = smartlist_create();

  obfsproxy_init();

  if (handle_environment(proxy) < 0)
    goto done;

  if (validate_environment(proxy) < 0)
    goto done;

  if (conf_proto_version_negotiation(proxy) < 0)
    goto done;

  if (launch_listeners(proxy) < 0)
    goto done;

  /* Kickstart libevent */
  event_base_dispatch(get_event_base());

  r=0;

 done:
  obfsproxy_cleanup();
  managed_proxy_free(proxy);

  return r;
}
