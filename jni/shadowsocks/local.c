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
#include <assert.h>
#include <linux/limits.h>

#include "local.h"
#include "socks5.h"
#include "android.h"

#define min(a,b) (((a)<(b))?(a):(b))

static char *_server;
static char *_remote_port;
static int   _timeout;
static char *_key;

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

    s = getaddrinfo("0.0.0.0", port, &hints, &result);
    if (s != 0) {
        LOGD("getaddrinfo: %s", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        listen_sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listen_sock < 0)
            continue;

        int opt = 1;
        int err = setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (err) {
            perror("setsocket");
        }

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
        char *buf = remote->buf;
        int *buf_len = &remote->buf_len;
        if (server->stage != 5) {
            buf = server->buf;
            buf_len = &server->buf_len;
        }

        ssize_t r = recv(server->fd, buf, BUF_SIZE, 0);

        if (r == 0) {
            // connection closed
            *buf_len = 0;
            close_and_free_remote(EV_A_ remote);
            close_and_free_server(EV_A_ server);
            return;
        } else if(r < 0) {
            if (errno == EAGAIN) {
                // no data
                // continue to wait for recv
                break;
            } else {
                perror("server recv");
                close_and_free_remote(EV_A_ remote);
                close_and_free_server(EV_A_ server);
                return;
            }
        }

        // local socks5 server
        if (server->stage == 5) {
            encrypt(remote->buf, r, server->e_ctx);
            int w = send(remote->fd, remote->buf, r, 0);
            if(w < 0) {
                if (errno == EAGAIN) {
                    // no data, wait for send
                    remote->buf_len = r;
                    ev_io_stop(EV_A_ &server_recv_ctx->io);
                    ev_io_start(EV_A_ &remote->send_ctx->io);
                    break;
                } else {
                    perror("send");
                    close_and_free_remote(EV_A_ remote);
                    close_and_free_server(EV_A_ server);
                    return;
                }
            } else if(w < r) {
                char *pt = remote->buf;
                char *et = pt + r;
                while (pt + w < et) {
                    *pt = *(pt + w);
                    pt++;
                }
                remote->buf_len = r - w;
                assert(remote->buf_len >= 0);
                ev_io_stop(EV_A_ &server_recv_ctx->io);
                ev_io_start(EV_A_ &remote->send_ctx->io);
                break;
            }
        } else if (server->stage == 0) {
            struct method_select_response response;
            response.ver = VERSION;
            response.method = 0;
            char *send_buf = (char *)&response;
            send(server->fd, send_buf, sizeof(response), 0);
            server->stage = 1;
            return;
        } else if (server->stage == 1) {
            struct socks5_request *request = (struct socks5_request *)server->buf;

            if (request->cmd != 1) {
                LOGE("unsupported cmd: %d\n", request->cmd);
                struct socks5_response response;
                response.ver = VERSION;
                response.rep = CMD_NOT_SUPPORTED;
                response.rsv = 0;
                response.atyp = 1;
                char *send_buf = (char *)&response;
                send(server->fd, send_buf, 4, 0);
                close_and_free_remote(EV_A_ remote);
                close_and_free_server(EV_A_ server);
                return;
            }

            char addr_to_send[256];
            unsigned char addr_len = 0;
            addr_to_send[addr_len++] = request->atyp;

            // get remote addr and port
            if (request->atyp == 1) {

                // IP V4
                size_t in_addr_len = sizeof(struct in_addr);
                memcpy(addr_to_send + addr_len, server->buf + 4, in_addr_len + 2);
                addr_len += in_addr_len + 2;

            } else if (request->atyp == 3) {
                // Domain name
                unsigned char name_len = *(unsigned char *)(server->buf + 4);
                addr_to_send[addr_len++] = name_len;
                memcpy(addr_to_send + addr_len, server->buf + 4 + 1, name_len);
                addr_len += name_len;

                // get port
                addr_to_send[addr_len++] = *(unsigned char *)(server->buf + 4 + 1 + name_len); 
                addr_to_send[addr_len++] = *(unsigned char *)(server->buf + 4 + 1 + name_len + 1); 
            } else {
                LOGE("unsupported addrtype: %d\n", request->atyp);
                close_and_free_remote(EV_A_ remote);
                close_and_free_server(EV_A_ server);
                return;
            }

            encrypt(addr_to_send, addr_len, server->e_ctx);
            send(remote->fd, addr_to_send, addr_len, 0);

            // Fake reply
            struct socks5_response response;
            response.ver = VERSION;
            response.rep = 0;
            response.rsv = 0;
            response.atyp = 1;

            struct in_addr sin_addr;
            inet_aton("0.0.0.0", &sin_addr);

            memcpy(server->buf, &response, 4);
            memcpy(server->buf + 4, &sin_addr, sizeof(struct in_addr));
            *((unsigned short *)(server->buf + 4 + sizeof(struct in_addr))) 
                = (unsigned short) htons(atoi(_remote_port));

            int reply_size = 4 + sizeof(struct in_addr) + sizeof(unsigned short);
            int r = send(server->fd, server->buf, reply_size, 0);
            if (r < reply_size) {
                LOGE("header not complete sent\n");
                close_and_free_remote(EV_A_ remote);
                close_and_free_server(EV_A_ server);
                return;
            }

            server->stage = 5;
        }
    }
}

static void server_send_cb (EV_P_ ev_io *w, int revents) {
    struct server_ctx *server_send_ctx = (struct server_ctx *)w;
    struct server *server = server_send_ctx->server;
    struct remote *remote = server->remote;
    if (server->buf_len == 0) {
        // close and free
        close_and_free_remote(EV_A_ remote);
        close_and_free_server(EV_A_ server);
        return;
    } else {
        // has data to send
        ssize_t r = send(server->fd, server->buf,
                server->buf_len, 0);
        if (r < 0) {
            if (errno != EAGAIN) {
                perror("send");
                close_and_free_remote(EV_A_ remote);
                close_and_free_server(EV_A_ server);
                return;
            }
            return;
        }
        if (r < server->buf_len) {
            // partly sent, move memory, wait for the next time to send
            char *pt = server->buf;
            char *et = pt + server->buf_len;
            while (pt + r < et) {
                *pt = *(pt + r);
                pt++;
            }
            server->buf_len -= r;
            assert(server->buf_len >= 0);
            return;
        } else {
            // all sent out, wait for reading
            ev_io_stop(EV_A_ &server_send_ctx->io);
            if (remote != NULL) {
                ev_io_start(EV_A_ &remote->recv_ctx->io);
            } else {
                close_and_free_remote(EV_A_ remote);
                close_and_free_server(EV_A_ server);
                return;
            }
        }
    }

}

static void remote_timeout_cb(EV_P_ ev_timer *watcher, int revents) {
    struct remote_ctx *remote_ctx = (struct remote_ctx *) (((void*)watcher)
            - sizeof(ev_io));
    struct remote *remote = remote_ctx->remote;
    struct server *server = remote->server;

    LOGD("remote timeout");

    ev_timer_stop(EV_A_ watcher);

    if (server == NULL) {
        close_and_free_remote(EV_A_ remote);
        return;
    }
    close_and_free_remote(EV_A_ remote);
    close_and_free_server(EV_A_ server);
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

        if (r == 0) {
            // connection closed
            server->buf_len = 0;
            close_and_free_remote(EV_A_ remote);
            close_and_free_server(EV_A_ server);
            return;
        } else if(r < 0) {
            if (errno == EAGAIN) {
                // no data
                // continue to wait for recv
                break;
            } else {
                perror("remote recv");
                close_and_free_remote(EV_A_ remote);
                close_and_free_server(EV_A_ server);
                return;
            }
        }
        decrypt(server->buf, r, server->d_ctx);
        int w = send(server->fd, server->buf, r, 0);
        if(w < 0) {
            if (errno == EAGAIN) {
                // no data, wait for send
                server->buf_len = r;
                ev_io_stop(EV_A_ &remote_recv_ctx->io);
                ev_io_start(EV_A_ &server->send_ctx->io);
                break;
            } else {
                perror("send");
                close_and_free_remote(EV_A_ remote);
                close_and_free_server(EV_A_ server);
                return;
            }
        } else if(w < r) {
            char *pt = server->buf;
            char *et = pt + r;
            while (pt + w < et) {
                *pt = *(pt + w);
                pt++;
            }
            server->buf_len = r - w;
            assert(server->buf_len >= 0);
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

        struct sockaddr_storage addr;
        socklen_t len = sizeof addr;
        int r = getpeername(remote->fd, (struct sockaddr*)&addr, &len);
        if (r == 0) {
            remote_send_ctx->connected = 1;
            ev_io_stop(EV_A_ &remote_send_ctx->io);
            ev_timer_stop(EV_A_ &remote_send_ctx->watcher);
            ev_io_start(EV_A_ &server->recv_ctx->io);
            ev_io_start(EV_A_ &remote->recv_ctx->io);
            return;
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
                if (errno != EAGAIN) {
                    perror("send");
                    // close and free
                    close_and_free_remote(EV_A_ remote);
                    close_and_free_server(EV_A_ server);
                    return;
                }
                return;
            }
            if (r < remote->buf_len) {
                // partly sent, move memory, wait for the next time to send
                char *pt = remote->buf;
                char *et = pt + remote->buf_len;
                while (pt + r < et) {
                    *pt = *(pt + r);
                    pt++;
                }
                remote->buf_len -= r;
                assert(remote->buf_len >= 0);
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
    remote->recv_ctx = malloc(sizeof(struct remote_ctx));
    remote->send_ctx = malloc(sizeof(struct remote_ctx));
    remote->fd = fd;
    ev_io_init(&remote->recv_ctx->io, remote_recv_cb, fd, EV_READ);
    ev_io_init(&remote->send_ctx->io, remote_send_cb, fd, EV_WRITE);
    ev_timer_init(&remote->send_ctx->watcher, remote_timeout_cb, _timeout, 0);
    remote->recv_ctx->remote = remote;
    remote->recv_ctx->connected = 0;
    remote->send_ctx->remote = remote;
    remote->send_ctx->connected = 0;
    remote->buf_len = 0;
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
        ev_timer_stop(EV_A_ &remote->send_ctx->watcher);
        ev_io_stop(EV_A_ &remote->send_ctx->io);
        ev_io_stop(EV_A_ &remote->recv_ctx->io);
        close(remote->fd);
        free_remote(remote);
    }
}
struct server* new_server(int fd) {
    struct server *server;
    server = malloc(sizeof(struct server));
    server->recv_ctx = malloc(sizeof(struct server_ctx));
    server->send_ctx = malloc(sizeof(struct server_ctx));
    server->fd = fd;
    ev_io_init(&server->recv_ctx->io, server_recv_cb, fd, EV_READ);
    ev_io_init(&server->send_ctx->io, server_send_cb, fd, EV_WRITE);
    server->recv_ctx->server = server;
    server->recv_ctx->connected = 0;
    server->send_ctx->server = server;
    server->send_ctx->connected = 0;
    server->stage = 0;
    if (_method == RC4) {
        server->e_ctx = malloc(sizeof(EVP_CIPHER_CTX));
        server->d_ctx = malloc(sizeof(EVP_CIPHER_CTX));
        enc_ctx_init(server->e_ctx, _key, 1);
        enc_ctx_init(server->d_ctx, _key, 0);
    } else {
        server->e_ctx = NULL;
        server->d_ctx = NULL;
    }
    server->buf_len = 0;
    return server;
}
void free_server(struct server *server) {
    if (server != NULL) {
        if (server->remote != NULL) {
            server->remote->server = NULL;
        }
        if (_method == RC4) {
            EVP_CIPHER_CTX_cleanup(server->e_ctx);
            EVP_CIPHER_CTX_cleanup(server->d_ctx);
            free(server->e_ctx);
            free(server->d_ctx);
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
        if (serverfd < 0) {
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
        int err = getaddrinfo(_server, _remote_port, &hints, &res);
        if (err) {
            perror("getaddrinfo");
            close_and_free_server(EV_A_ server);
            break;
        }

        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0) {
            perror("socket");
            close(sockfd);
            close_and_free_server(EV_A_ server);
            freeaddrinfo(res);
            break;
        }

        struct timeval timeout;
        timeout.tv_sec = _timeout;
        timeout.tv_usec = 0;
        err = setsockopt(sockfd, SOL_SOCKET, 
                SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
        if (err) perror("setsockopt");
        err = setsockopt(sockfd, SOL_SOCKET,
                SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
        if (err) perror("setsockopt");

        setnonblocking(sockfd);
        struct remote *remote = new_remote(sockfd);
        server->remote = remote;
        remote->server = server;
        connect(sockfd, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        // listen to remote connected event
        ev_io_start(EV_A_ &remote->send_ctx->io);
        ev_timer_start(EV_A_ &remote->send_ctx->watcher);
        break;
    }
}

int main (int argc, char **argv)
{

    char *server = NULL;
    char *remote_port = NULL;
    char *port = NULL;
    char *key = NULL;
    char* timeout = "10";
    char* method = NULL;
    int c;
    int f_flags = 0;

    opterr = 0;

    while ((c = getopt (argc, argv, "fs:p:l:k:t:m:")) != -1) {
        switch (c) {
            case 's':
                server = optarg;
                break;
            case 'p':
                remote_port = optarg;
                break;
            case 'l':
                port = optarg;
                break;
            case 'k':
                key = optarg;
                break;
            case 'f':
                f_flags = 1;
                break;
            case 't':
                timeout = optarg;
                break;
            case 'm':
                method = optarg;
                break;
        }
    }

    if (server == NULL || remote_port == NULL ||
            port == NULL || key == NULL) {
        exit(EXIT_FAILURE);
    }

    if (f_flags) {

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
    } else {
        pid_t pid = getpid();
        FILE *file = fopen("/data/data/com.github.shadowsocks/shadowsocks.pid", "w");
        fprintf(file, "%d", pid);
        fclose(file);
    }

    signal(SIGPIPE, SIG_IGN);

    // init global variables
    _server = strdup(server);
    _remote_port = strdup(remote_port);
    _timeout = atoi(timeout);
    _key = key;
    _method = TABLE;
    if (method != NULL) {
        if (strcmp(method, "rc4") == 0) {
            _method = RC4;
        }
    }

    LOGD("calculating ciphers %d", _method);
    if (_method != RC4) {
        get_table(key);
    }

    int listenfd;
    listenfd = create_and_bind(port);
    if (listenfd < 0) {
        LOGE("bind() error..");
        return 1;
    }
    if (listen(listenfd, SOMAXCONN) < 0) {
        LOGE("listen() error.");
        return 1;
    }
    LOGD("server listening at port %s\n", port);

    setnonblocking(listenfd);
    struct listen_ctx listen_ctx;
    listen_ctx.fd = listenfd;
    struct ev_loop *loop = ev_default_loop(EVBACKEND_EPOLL | EVFLAG_NOENV);
    if (!loop) {
        LOGE("no epoll found here, maybe it hides under your chair");
        return 1;
    }
    ev_io_init (&listen_ctx.io, accept_cb, listenfd, EV_READ);
    ev_io_start (loop, &listen_ctx.io);
    ev_run (loop, 0);
    return 0;
}

