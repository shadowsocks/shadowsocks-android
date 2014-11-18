#ifndef REDSOCKS_H_WED_JAN_24_22_17_11_2007
#define REDSOCKS_H_WED_JAN_24_22_17_11_2007
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <event.h>
#include "list.h"


struct redsocks_client_t;
struct redsocks_instance_t;

typedef struct relay_subsys_t {
	char   *name;
	size_t  payload_len; // size of relay-specific data in client section
	size_t  instance_payload_len; // size of relay-specify data in instance section
	evbuffercb readcb;
	evbuffercb writecb;
	void       (*init)(struct redsocks_client_t *client);
	void       (*fini)(struct redsocks_client_t *client);
	void       (*instance_fini)(struct redsocks_instance_t *instance);
	// connect_relay (if any) is called instead of redsocks_connect_relay after client connection acceptance
	void       (*connect_relay)(struct redsocks_client_t *client);
} relay_subsys;

typedef struct redsocks_config_t {
	struct sockaddr_in bindaddr;
	struct sockaddr_in relayaddr;
	char *type;
	char *login;
	char *password;
	uint16_t min_backoff_ms;
	uint16_t max_backoff_ms; // backoff capped by 65 seconds is enough :)
	uint16_t listenq;
} redsocks_config;

struct tracked_event {
	struct event ev;
	struct timeval inserted;
};

typedef struct redsocks_instance_t {
	list_head       list;
	redsocks_config config;
	struct tracked_event listener;
	struct tracked_event accept_backoff;
	uint16_t        accept_backoff_ms;
	list_head       clients;
	relay_subsys   *relay_ss;
} redsocks_instance;

typedef struct redsocks_client_t {
	list_head           list;
	redsocks_instance  *instance;
	struct bufferevent *client;
	struct bufferevent *relay;
	struct sockaddr_in  clientaddr;
	struct sockaddr_in  destaddr;
	int                 state;         // it's used by bottom layer
	unsigned short      client_evshut;
	unsigned short      relay_evshut;
	time_t              first_event;
	time_t              last_event;
} redsocks_client;


void redsocks_drop_client(redsocks_client *client);
void redsocks_touch_client(redsocks_client *client);
void redsocks_connect_relay(redsocks_client *client);
void redsocks_start_relay(redsocks_client *client);

typedef int (*size_comparator)(size_t a, size_t b);
int sizes_equal(size_t a, size_t b);
int sizes_greater_equal(size_t a, size_t b);
/** helper for functions when we expect ONLY reply of some size and anything else is error
 */
int redsocks_read_expected(redsocks_client *client, struct evbuffer *input, void *data, size_comparator comparator, size_t expected);

typedef struct evbuffer* (*redsocks_message_maker)(redsocks_client *client);
typedef struct evbuffer* (*redsocks_message_maker_plain)(void *p);
struct evbuffer *mkevbuffer(void *data, size_t len);
/* Yahoo! This code is ex-plain! :-D */
int redsocks_write_helper_ex_plain(
	struct bufferevent *buffev, redsocks_client *client,
	redsocks_message_maker_plain mkmessage, void *p, int state, size_t wm_low, size_t wm_high);
int redsocks_write_helper_ex(
	struct bufferevent *buffev, redsocks_client *client,
	redsocks_message_maker mkmessage, int state, size_t wm_low, size_t wm_high);
int redsocks_write_helper(
	struct bufferevent *buffev, redsocks_client *client,
	redsocks_message_maker mkmessage, int state, size_t wm_only);


#define redsocks_close(fd) redsocks_close_internal((fd), __FILE__, __LINE__, __func__)
void redsocks_close_internal(int fd, const char* file, int line, const char *func);

#define redsocks_log_error(client, prio, msg...) \
	redsocks_log_write_plain(__FILE__, __LINE__, __func__, 0, &(client)->clientaddr, &(client)->destaddr, prio, ## msg)
#define redsocks_log_errno(client, prio, msg...) \
	redsocks_log_write_plain(__FILE__, __LINE__, __func__, 1, &(client)->clientaddr, &(client)->destaddr, prio, ## msg)
void redsocks_log_write_plain(
		const char *file, int line, const char *func, int do_errno,
		const struct sockaddr_in *clientaddr, const struct sockaddr_in *destaddr,
		int priority, const char *fmt, ...)
#if defined(__GNUC__)
	__attribute__ (( format (printf, 8, 9) ))
#endif
;

/* vim:set tabstop=4 softtabstop=4 shiftwidth=4: */
/* vim:set foldmethod=marker foldlevel=32 foldmarker={,}: */
#endif /* REDSOCKS_H_WED_JAN_24_22_17_11_2007 */

