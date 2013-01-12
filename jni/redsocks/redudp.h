#ifndef REDUDP_H
#define REDUDP_H

typedef struct redudp_config_t {
	struct sockaddr_in bindaddr;
	struct sockaddr_in relayaddr;
	// TODO:           outgoingaddr;
	struct sockaddr_in destaddr;
	char *login;
	char *password;
	uint16_t max_pktqueue;
	uint16_t udp_timeout;
	uint16_t udp_timeout_stream;
} redudp_config;

typedef struct redudp_instance_t {
	list_head       list;
	redudp_config   config;
	struct event    listener;
	list_head       clients;
} redudp_instance;

typedef struct redudp_client_t {
	list_head           list;
	redudp_instance    *instance;
	struct sockaddr_in  clientaddr;
	struct sockaddr_in  destaddr;
	int                 sender_fd; // shared between several clients socket (bound to `destaddr`)
	struct event        timeout;
	struct bufferevent *relay;
	struct event        udprelay;
	struct sockaddr_in  udprelayaddr;
	int                 state;         // it's used by bottom layer
	time_t              first_event;
	time_t              last_client_event;
	time_t              last_relay_event;
	unsigned int        queue_len;
	list_head           queue;
} redudp_client;

typedef struct enqueued_packet_t {
	list_head  list;
	size_t     len;
	char       data[1];
} enqueued_packet;

/* vim:set tabstop=4 softtabstop=4 shiftwidth=4: */
/* vim:set foldmethod=marker foldlevel=32 foldmarker={,}: */
#endif /* REDUDP_H */
