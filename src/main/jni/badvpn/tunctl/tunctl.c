#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <pwd.h>
#include <grp.h>

#include <misc/version.h>
#include <misc/open_standard_streams.h>

#define PROGRAM_NAME "tunctl"

#define TUN_DEVNODE "/dev/net/tun"

struct {
    int help;
    int version;
    int op;
    char *device_name;
    char *user;
    char *group;
} options;

#define OP_MKTUN 1
#define OP_MKTAP 2
#define OP_RMTUN 3
#define OP_RMTAP 4

static void print_help (const char *name);
static void print_version (void);
static int parse_arguments (int argc, char *argv[]);
static int make_tuntap (const char *ifname, int is_tun, const char *user, const char *group);
static int remove_tuntap (const char *ifname, int is_tun);

int main (int argc, char *argv[])
{
    int res = 1;
    
    // open standard streams
    open_standard_streams();
    
    // parse command-line arguments
    if (!parse_arguments(argc, argv)) {
        fprintf(stderr, "Error: Failed to parse arguments\n");
        print_help(argv[0]);
        goto fail0;
    }
    
    // handle --help and --version
    if (options.help) {
        print_version();
        print_help(argv[0]);
        return 0;
    }
    if (options.version) {
        print_version();
        return 0;
    }
    
    if (options.op == OP_MKTUN || options.op == OP_MKTAP) {
        if (!options.user && !options.group) {
            fprintf(stderr, "WARNING: with neither --user nor --group, anyone will be able to use the device!\n");
        }
        res = !make_tuntap(options.device_name, options.op == OP_MKTUN, options.user, options.group);
    } else {
        res = !remove_tuntap(options.device_name, options.op == OP_RMTUN);
    }
    
fail0:
    return res;
}

void print_help (const char *name)
{
    printf(
        "Usage:\n"
        "    %s [--help] [--version]\n"
        "    %s --mktun <device_name> [--user <username>] [--group <groupname>]\n"
        "    %s --mktap <device_name> [--user <username>] [--group <groupname>]\n"
        "    %s --rmtun <device_name>\n"
        "    %s --rmtap <device_name>\n",
        name, name, name, name, name
    );
}

void print_version (void)
{
    printf(GLOBAL_PRODUCT_NAME" "PROGRAM_NAME" "GLOBAL_VERSION"\n"GLOBAL_COPYRIGHT_NOTICE"\n");
}

int parse_arguments (int argc, char *argv[])
{
    if (argc <= 0) {
        return 0;
    }
    
    options.help = 0;
    options.version = 0;
    options.op = -1;
    options.device_name = NULL;
    options.user = NULL;
    options.group = NULL;
    
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (!strcmp(arg, "--help")) {
            options.help = 1;
        }
        else if (!strcmp(arg, "--version")) {
            options.version = 1;
        }
        else if (!strcmp(arg, "--mktun")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if (options.op >= 0) {
                fprintf(stderr, "%s: can only do one operation\n", arg);
                return 0;
            }
            options.op = OP_MKTUN;
            options.device_name = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--mktap")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if (options.op >= 0) {
                fprintf(stderr, "%s: can only do one operation\n", arg);
                return 0;
            }
            options.op = OP_MKTAP;
            options.device_name = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--rmtun")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if (options.op >= 0) {
                fprintf(stderr, "%s: can only do one operation\n", arg);
                return 0;
            }
            options.op = OP_RMTUN;
            options.device_name = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--rmtap")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            if (options.op >= 0) {
                fprintf(stderr, "%s: can only do one operation\n", arg);
                return 0;
            }
            options.op = OP_RMTAP;
            options.device_name = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--user")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.user = argv[i + 1];
            i++;
        }
        else if (!strcmp(arg, "--group")) {
            if (1 >= argc - i) {
                fprintf(stderr, "%s: requires an argument\n", arg);
                return 0;
            }
            options.group = argv[i + 1];
            i++;
        }
        else {
            fprintf(stderr, "unknown option: %s\n", arg);
            return 0;
        }
    }
    
    if (options.help || options.version) {
        return 1;
    }
    
    if (options.op < 0) {
        fprintf(stderr, "--mktun, --mktap --rmtun or --rmtap is required\n");
        return 0;
    }
    
    if ((options.user || options.group) && options.op != OP_MKTUN && options.op != OP_MKTAP) {
        fprintf(stderr, "--user and --group only make sense for --mktun and --mktap\n");
        return 0;
    }
    
    return 1;
}

static int make_tuntap (const char *ifname, int is_tun, const char *user, const char *group)
{
    int res = 0;
    
    if (strlen(ifname) >= IFNAMSIZ) {
        fprintf(stderr, "Error: ifname too long\n");
        goto fail0;
    }
    
    int fd = open(TUN_DEVNODE, O_RDWR);
    if (fd < 0) {
        perror("open");
        fprintf(stderr, "Error: open tun failed\n");
        goto fail0;
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = (is_tun ? IFF_TUN : IFF_TAP);
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    
    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        perror("ioctl(TUNSETIFF)");
        fprintf(stderr, "Error: TUNSETIFF failed\n");
        goto fail1;
    }
    
    uid_t uid = -1;
    gid_t gid = -1;
    
    if (user) {
        long bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
        if (bufsize < 0) {
            bufsize = 16384;
        }
        
        char *buf = malloc(bufsize);
        if (!buf) {
            fprintf(stderr, "Error: malloc failed\n");
            goto fail1;
        }
        
        struct passwd pwd;
        struct passwd *res;
        getpwnam_r(user, &pwd, buf, bufsize, &res);
        if (!res) {
            fprintf(stderr, "Error: getpwnam_r failed\n");
            free(buf);
            goto fail1;
        }
        
        uid = pwd.pw_uid;
        free(buf);
    }
    
    if (group) {
        long bufsize = sysconf(_SC_GETGR_R_SIZE_MAX);
        if (bufsize < 0) {
            bufsize = 16384;
        }
        
        char *buf = malloc(bufsize);
        if (!buf) {
            fprintf(stderr, "Error: malloc failed\n");
            goto fail1;
        }
        
        struct group grp;
        struct group *res;
        getgrnam_r(group, &grp, buf, bufsize, &res);
        if (!res) {
            fprintf(stderr, "Error: getgrnam_r failed\n");
            free(buf);
            goto fail1;
        }
        
        gid = grp.gr_gid;
        free(buf);
    }
    
    if (ioctl(fd, TUNSETOWNER, uid) < 0) {
        perror("ioctl(TUNSETOWNER)");
        fprintf(stderr, "Error: TUNSETOWNER failed\n");
        goto fail1;
    }
    
    if (ioctl(fd, TUNSETGROUP, gid) < 0) {
        perror("ioctl(TUNSETGROUP)");
        fprintf(stderr, "Error: TUNSETGROUP failed\n");
        goto fail1;
    }
    
    if (ioctl(fd, TUNSETPERSIST, (void *)1) < 0) {
        perror("ioctl(TUNSETPERSIST)");
        fprintf(stderr, "Error: TUNSETPERSIST failed\n");
        goto fail1;
    }
    
    res = 1;
    
fail1:
    close(fd);
fail0:
    return res;
}

static int remove_tuntap (const char *ifname, int is_tun)
{
    int res = 0;
    
    if (strlen(ifname) >= IFNAMSIZ) {
        fprintf(stderr, "Error: ifname too long\n");
        goto fail0;
    }
    
    int fd = open(TUN_DEVNODE, O_RDWR);
    if (fd < 0) {
        perror("open");
        fprintf(stderr, "Error: open tun failed\n");
        goto fail0;
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = (is_tun ? IFF_TUN : IFF_TAP);
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    
    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        perror("ioctl(TUNSETIFF)");
        fprintf(stderr, "Error: TUNSETIFF failed\n");
        goto fail1;
    }
    
    if (ioctl(fd, TUNSETPERSIST, (void *)0) < 0) {
        perror("ioctl(TUNSETPERSIST)");
        fprintf(stderr, "Error: TUNSETPERSIST failed\n");
        goto fail1;
    }
    
    res = 1;
    
fail1:
    close(fd);
fail0:
    return res;
}
