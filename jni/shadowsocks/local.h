#pragma once

#include <ev.h>

#define BUF_SIZE 1500

struct listen_ctx {
	ev_io io;
	int fd;
	struct sockaddr sock;
};

struct server {
	int fd;
	char buf[BUF_SIZE]; // server send from, remote recv into
    char stage;
	int buf_len;
	struct server_ctx *recv_ctx;
	struct server_ctx *send_ctx;
	struct remote *remote;
};
struct server_ctx {
	ev_io io;
	int connected;
	struct server *server;
};
struct remote {
	int fd;
	char buf[BUF_SIZE]; // remote send from, server recv into
	int buf_len;
	struct remote_ctx *recv_ctx;
	struct remote_ctx *send_ctx;
	struct server *server;
};
struct remote_ctx {
	ev_io io;
	int connected;
	struct remote *remote;
};


static void accept_cb (EV_P_ ev_io *w, int revents);
static void server_recv_cb (EV_P_ ev_io *w, int revents);
static void server_send_cb (EV_P_ ev_io *w, int revents);
static void remote_recv_cb (EV_P_ ev_io *w, int revents);
static void remote_send_cb (EV_P_ ev_io *w, int revents);
struct remote* new_remote(int fd);
void free_remote(struct remote *remote);
void close_and_free_remote(EV_P_ struct remote *remote);
struct server* new_server(int fd);
void free_server(struct server *server);
void close_and_free_server(EV_P_ struct server *server);

