#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <linux/limits.h>

#include "local.h"
#include "encrypt.h"
#include "android.h"

#define REPLY "HTTP/1.1 200 OK\n\nhello"

#define min(a,b) (((a)<(b))?(a):(b))

// every watcher type has its own typedef'd struct
// with the name ev_TYPE
ev_io stdin_watcher;

static char *_server;
static char *_remote_port;

struct client_ctx {
    ev_io io;
    int fd;
};

int setnonblocking(int fd) {
    int flags;
    if (-1 ==(flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int create_and_bind(const char *port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, listen_sock;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC; /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
    hints.ai_flags = AI_PASSIVE; /* All interfaces */

    s = getaddrinfo("0.0.0.0", port, &hints, &result);
    if (s != 0) {
        LOGD("getaddrinfo: %s", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        listen_sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (listen_sock == -1)
            continue;

        s = bind(listen_sock, rp->ai_addr, rp->ai_addrlen);
        if (s == 0) {
            /* We managed to bind successfully! */
            break;
        } else {
            perror("bind");
        }

        close(listen_sock);
    }

    if (rp == NULL) {
        LOGE("Could not bind");
        return -1;
    }

    freeaddrinfo(result);

    return listen_sock;
}

static void server_recv_cb (EV_P_ ev_io *w, int revents) {
    struct server_ctx *server_recv_ctx = (struct server_ctx *)w;
    struct server *server = server_recv_ctx->server;
    struct remote *remote = server->remote;
    if (remote == NULL) {
        close_and_free_server(EV_A_ server);
        return;
    }
    while (1) {
        ssize_t r = recv(server->fd, remote->buf, BUF_SIZE, 0);
        if (r == 0) {
            // connection closed
            remote->buf_len = 0;
            close_and_free_server(EV_A_ server);
            if (remote != NULL) {
                ev_io_start(EV_A_ &remote->send_ctx->io);
            }
            return;
        } else if(r < 0) {
            perror("recv");
            if (errno == EAGAIN) {
                // no data
                // continue to wait for recv
                break;
            } else {
                close_and_free_server(EV_A_ server);
                close_and_free_remote(EV_A_ remote);
                return;
            }
        }
        encrypt(remote->buf, r);
        int w = send(remote->fd, remote->buf, r, MSG_NOSIGNAL);
        if(w == -1) {
            perror("send");
            if (errno == EAGAIN) {
                // no data, wait for send
                ev_io_stop(EV_A_ &server_recv_ctx->io);
                ev_io_start(EV_A_ &remote->send_ctx->io);
                break;
            } else {
                close_and_free_server(EV_A_ server);
                close_and_free_remote(EV_A_ remote);
                return;
            }
        } else if(w < r) {
            char *pt;
            for (pt = remote->buf; pt < pt + min(w, BUF_SIZE); pt++) {
                *pt = *(pt + w);
            }
            remote->buf_len = r - w;
            ev_io_stop(EV_A_ &server_recv_ctx->io);
            ev_io_start(EV_A_ &remote->send_ctx->io);
            break;
        }
    }
}
static void server_send_cb (EV_P_ ev_io *w, int revents) {
    struct server_ctx *server_send_ctx = (struct server_ctx *)w;
    struct server *server = server_send_ctx->server;
    struct remote *remote = server->remote;
    if (server->buf_len == 0) {
        // close and free
        close_and_free_server(EV_A_ server);
        close_and_free_remote(EV_A_ remote);
        return;
    } else {
        // has data to send
        ssize_t r = send(server->fd, server->buf,
                server->buf_len, 0);
        if (r < 0) {
            perror("send");
            if (errno != EAGAIN) {
                close_and_free_server(EV_A_ server);
                close_and_free_remote(EV_A_ remote);
                return;
            }
            return;
        }
        if (r < server->buf_len) {
            LOGD("r=%d\n", r);
            LOGD("server->buf_len=%d\n", server->buf_len);
            // partly sent, move memory, wait for the next time to send
            char *pt;
            for (pt = server->buf; pt < pt + min(r, BUF_SIZE); pt++) {
                *pt = *(pt + r);
            }
            server->buf_len -= r;
            return;
        } else {
            // all sent out, wait for reading
            ev_io_stop(EV_A_ &server_send_ctx->io);
            if (remote != NULL) {
                ev_io_start(EV_A_ &remote->recv_ctx->io);
            } else {
                close_and_free_server(EV_A_ server);
                close_and_free_remote(EV_A_ remote);
                return;
            }
        }
    }

}
static void remote_recv_cb (EV_P_ ev_io *w, int revents) {
    struct remote_ctx *remote_recv_ctx = (struct remote_ctx *)w;
    struct remote *remote = remote_recv_ctx->remote;
    struct server *server = remote->server;
    if (server == NULL) {
        close_and_free_remote(EV_A_ remote);
        return;
    }
    while (1) {
        ssize_t r = recv(remote->fd, server->buf, BUF_SIZE, 0);
        /*LOGD("after recv: r=%d\n", r);*/
        if (r == 0) {
            // connection closed
            server->buf_len = 0;
            close_and_free_remote(EV_A_ remote);
            if (server != NULL) {
                ev_io_start(EV_A_ &server->send_ctx->io);
            }
            return;
        } else if(r < 0) {
            perror("recv");
            if (errno == EAGAIN) {
                // no data
                // continue to wait for recv
                break;
            } else {
                close_and_free_server(EV_A_ server);
                close_and_free_remote(EV_A_ remote);
                return;
            }
        }
        decrypt(server->buf, r);
        int w = send(server->fd, server->buf, r, MSG_NOSIGNAL);
        /*LOGD("after send: w=%d\n", w);*/
        if(w == -1) {
            perror("send");
            if (errno == EAGAIN) {
                // no data, wait for send
                ev_io_stop(EV_A_ &remote_recv_ctx->io);
                ev_io_start(EV_A_ &server->send_ctx->io);
                break;
            } else {
                close_and_free_server(EV_A_ server);
                close_and_free_remote(EV_A_ remote);
                return;
            }
        } else if(w < r) {
            char *pt;
            for (pt = server->buf; pt < pt + min(w, BUF_SIZE); pt++) {
                *pt = *(pt + w);
            }
            server->buf_len = r - w;
            ev_io_stop(EV_A_ &remote_recv_ctx->io);
            ev_io_start(EV_A_ &server->send_ctx->io);
            break;
        }
    }
}
static void remote_send_cb (EV_P_ ev_io *w, int revents) {
    struct remote_ctx *remote_send_ctx = (struct remote_ctx *)w;
    struct remote *remote = remote_send_ctx->remote;
    struct server *server = remote->server;
    if (!remote_send_ctx->connected) {
        socklen_t len;
        struct sockaddr_storage addr;
        len = sizeof addr;
        int r = getpeername(remote->fd, (struct sockaddr*)&addr, &len);
        if (r == 0) {
            remote_send_ctx->connected = 1;
            ev_io_stop(EV_A_ &remote_send_ctx->io);
            ev_io_start(EV_A_ &server->recv_ctx->io);
            ev_io_start(EV_A_ &remote->recv_ctx->io);
        } else {
            perror("getpeername");
            // not connected
            close_and_free_remote(EV_A_ remote);
            close_and_free_server(EV_A_ server);
            return;
        }
    } else {
        if (remote->buf_len == 0) {
            // close and free
            close_and_free_remote(EV_A_ remote);
            close_and_free_server(EV_A_ server);
            return;
        } else {
            // has data to send
            ssize_t r = send(remote->fd, remote->buf,
                    remote->buf_len, 0);
            if (r < 0) {
                perror("send");
                if (errno != EAGAIN) {
                    // close and free
                    close_and_free_remote(EV_A_ remote);
                    close_and_free_server(EV_A_ server);
                    return;
                }
                return;
            }
            if (r < remote->buf_len) {
                // partly sent, move memory, wait for the next time to send
                char *pt;
                for (pt = remote->buf; pt < pt + min(r, BUF_SIZE); pt++) {
                    *pt = *(pt + r);
                }
                remote->buf_len -= r;
                return;
            } else {
                // all sent out, wait for reading
                ev_io_stop(EV_A_ &remote_send_ctx->io);
                if (server != NULL) {
                    ev_io_start(EV_A_ &server->recv_ctx->io);
                } else {
                    close_and_free_remote(EV_A_ remote);
                    close_and_free_server(EV_A_ server);
                    return;
                }
            }
        }

    }
}

struct remote* new_remote(int fd) {
    struct remote *remote;
    remote = malloc(sizeof(struct remote));
    remote->fd = fd;
    remote->recv_ctx = malloc(sizeof(struct remote_ctx));
    remote->send_ctx = malloc(sizeof(struct remote_ctx));
    ev_io_init(&remote->recv_ctx->io, remote_recv_cb, fd, EV_READ);
    ev_io_init(&remote->send_ctx->io, remote_send_cb, fd, EV_WRITE);
    remote->recv_ctx->remote = remote;
    remote->recv_ctx->connected = 0;
    remote->send_ctx->remote = remote;
    remote->send_ctx->connected = 0;
    return remote;
}
void free_remote(struct remote *remote) {
    if (remote != NULL) {
        if (remote->server != NULL) {
            remote->server->remote = NULL;
        }
        free(remote->recv_ctx);
        free(remote->send_ctx);
        free(remote);
    }
}
void close_and_free_remote(EV_P_ struct remote *remote) {
    if (remote != NULL) {
        ev_io_stop(EV_A_ &remote->send_ctx->io);
        ev_io_stop(EV_A_ &remote->recv_ctx->io);
        close(remote->fd);
        free_remote(remote);
    }
}
struct server* new_server(int fd) {
    struct server *server;
    server = malloc(sizeof(struct server));
    server->fd = fd;
    server->recv_ctx = malloc(sizeof(struct server_ctx));
    server->send_ctx = malloc(sizeof(struct server_ctx));
    ev_io_init(&server->recv_ctx->io, server_recv_cb, fd, EV_READ);
    ev_io_init(&server->send_ctx->io, server_send_cb, fd, EV_WRITE);
    server->recv_ctx->server = server;
    server->recv_ctx->connected = 0;
    server->send_ctx->server = server;
    server->send_ctx->connected = 0;
    return server;
}
void free_server(struct server *server) {
    if (server != NULL) {
        if (server->remote != NULL) {
            server->remote->server = NULL;
        }
        free(server->recv_ctx);
        free(server->send_ctx);
        free(server);
    }
}
void close_and_free_server(EV_P_ struct server *server) {
    if (server != NULL) {
        ev_io_stop(EV_A_ &server->send_ctx->io);
        ev_io_stop(EV_A_ &server->recv_ctx->io);
        close(server->fd);
        free_server(server);
    }
}

static void accept_cb (EV_P_ ev_io *w, int revents)
{
    struct listen_ctx *listener = (struct listen_ctx *)w;
    int serverfd;
    while (1) {
        serverfd = accept(listener->fd, NULL, NULL);
        if (serverfd == -1) {
            perror("accept");
            break;
        }
        setnonblocking(serverfd);
        struct server *server = new_server(serverfd);
        struct addrinfo hints, *res;
        int sockfd;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        getaddrinfo(_server, _remote_port, &hints, &res);
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0) {
            perror("socket");
            close(sockfd);
            free_server(server);
            continue;
        }
        setnonblocking(sockfd);
        struct remote *remote = new_remote(sockfd);
        server->remote = remote;
        remote->server = server;
        connect(sockfd, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        // listen to remote connected event
        ev_io_start(EV_A_ &remote->send_ctx->io);
        break;
    }
}

int main (int argc, char **argv)
{

    if (argc < 5)
        return -1;

    const char *server = argv[1];
    const char *remote_port = argv[2];
    const char *port = argv[3];
    const char *key =  argv[4];

    /* Our process ID and Session ID */
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then
       we can exit the parent process. */
    if (pid > 0) {
        FILE *file = fopen("/data/data/com.github.shadowsocks/shadowsocks.pid", "w");
        fprintf(file, "%d", pid);
        fclose(file);
        exit(EXIT_SUCCESS);
    }

    /* Change the file mode mask */
    umask(0);

    /* Open any logs here */        

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        /* Log the failure */
        exit(EXIT_FAILURE);
    }


    /* Change the current working directory */
    if ((chdir("/")) < 0) {
        /* Log the failure */
        exit(EXIT_FAILURE);
    }

    /* Close out the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    _server = strdup(server);
    _remote_port = strdup(remote_port);

    LOGD("calculating ciphers");
    get_table(key);

    int listenfd;
    listenfd = create_and_bind(port);
    if (listenfd < 0) {
        LOGE("bind() error..");
        return 1;
    }
    if (listen(listenfd, SOMAXCONN) == -1) {
        LOGE("listen() error.");
        return 1;
    }
    LOGD("server listening at port %s\n", port);

    setnonblocking(listenfd);
    struct listen_ctx listen_ctx;
    listen_ctx.fd = listenfd;
    struct ev_loop *loop = EV_DEFAULT;
    ev_io_init (&listen_ctx.io, accept_cb, listenfd, EV_READ);
    ev_io_start (loop, &listen_ctx.io);
    ev_run (loop, 0);
    return 0;
}

