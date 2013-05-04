#ifndef DNSTC_H
#define DNSTC_H

typedef struct dnstc_config_t {
	struct sockaddr_in bindaddr;
} dnstc_config;

typedef struct dnstc_instance_t {
	list_head       list;
	dnstc_config    config;
	struct event    listener;
} dnstc_instance;

/* vim:set tabstop=4 softtabstop=4 shiftwidth=4: */
/* vim:set foldmethod=marker foldlevel=32 foldmarker={,}: */
#endif /* REDUDP_H */
