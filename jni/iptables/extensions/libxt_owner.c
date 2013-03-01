/*
 *	libxt_owner - iptables addon for xt_owner
 *
 *	Copyright © CC Computer Consultants GmbH, 2007 - 2008
 *	Jan Engelhardt <jengelh@computergmbh.de>
 */
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>
#include <xtables.h>
#include <linux/netfilter/xt_owner.h>

/* match and invert flags */
enum {
	IPT_OWNER_UID   = 0x01,
	IPT_OWNER_GID   = 0x02,
	IPT_OWNER_PID   = 0x04,
	IPT_OWNER_SID   = 0x08,
	IPT_OWNER_COMM  = 0x10,
	IP6T_OWNER_UID  = IPT_OWNER_UID,
	IP6T_OWNER_GID  = IPT_OWNER_GID,
	IP6T_OWNER_PID  = IPT_OWNER_PID,
	IP6T_OWNER_SID  = IPT_OWNER_SID,
	IP6T_OWNER_COMM = IPT_OWNER_COMM,
};

struct ipt_owner_info {
	uid_t uid;
	gid_t gid;
	pid_t pid;
	pid_t sid;
	char comm[16];
	uint8_t match, invert;	/* flags */
};

struct ip6t_owner_info {
	uid_t uid;
	gid_t gid;
	pid_t pid;
	pid_t sid;
	char comm[16];
	uint8_t match, invert;	/* flags */
};

/*
 *	Note: "UINT32_MAX - 1" is used in the code because -1 is a reserved
 *	UID/GID value anyway.
 */

enum {
	O_USER = 0,
	O_GROUP,
	O_SOCK_EXISTS,
	O_PROCESS,
	O_SESSION,
	O_COMM,
};

static void owner_mt_help_v0(void)
{
	printf(
"owner match options:\n"
"[!] --uid-owner userid       Match local UID\n"
"[!] --gid-owner groupid      Match local GID\n"
"[!] --pid-owner processid    Match local PID\n"
"[!] --sid-owner sessionid    Match local SID\n"
"[!] --cmd-owner name         Match local command name\n"
"NOTE: PID, SID and command matching are broken on SMP\n");
}

static void owner_mt6_help_v0(void)
{
	printf(
"owner match options:\n"
"[!] --uid-owner userid       Match local UID\n"
"[!] --gid-owner groupid      Match local GID\n"
"[!] --pid-owner processid    Match local PID\n"
"[!] --sid-owner sessionid    Match local SID\n"
"NOTE: PID and SID matching are broken on SMP\n");
}

static void owner_mt_help(void)
{
	printf(
"owner match options:\n"
"[!] --uid-owner userid[-userid]      Match local UID\n"
"[!] --gid-owner groupid[-groupid]    Match local GID\n"
"[!] --socket-exists                  Match if socket exists\n");
}

#define s struct ipt_owner_info
static const struct xt_option_entry owner_mt_opts_v0[] = {
	{.name = "uid-owner", .id = O_USER, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "gid-owner", .id = O_GROUP, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "pid-owner", .id = O_PROCESS, .type = XTTYPE_UINT32,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, pid),
	 .max = INT_MAX},
	{.name = "sid-owner", .id = O_SESSION, .type = XTTYPE_UINT32,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, sid),
	 .max = INT_MAX},
	{.name = "cmd-owner", .id = O_COMM, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, comm)},
	XTOPT_TABLEEND,
};
#undef s

#define s struct ip6t_owner_info
static const struct xt_option_entry owner_mt6_opts_v0[] = {
	{.name = "uid-owner", .id = O_USER, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "gid-owner", .id = O_GROUP, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "pid-owner", .id = O_PROCESS, .type = XTTYPE_UINT32,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, pid),
	 .max = INT_MAX},
	{.name = "sid-owner", .id = O_SESSION, .type = XTTYPE_UINT32,
	 .flags = XTOPT_INVERT | XTOPT_PUT, XTOPT_POINTER(s, sid),
	 .max = INT_MAX},
	XTOPT_TABLEEND,
};
#undef s

static const struct xt_option_entry owner_mt_opts[] = {
	{.name = "uid-owner", .id = O_USER, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "gid-owner", .id = O_GROUP, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "socket-exists", .id = O_SOCK_EXISTS, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};

static void owner_mt_parse_v0(struct xt_option_call *cb)
{
	struct ipt_owner_info *info = cb->data;
	struct passwd *pwd;
	struct group *grp;
	unsigned int id;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_USER:
		if ((pwd = getpwnam(cb->arg)) != NULL)
			id = pwd->pw_uid;
		else if (!xtables_strtoui(cb->arg, NULL, &id, 0, UINT32_MAX - 1))
			xtables_param_act(XTF_BAD_VALUE, "owner", "--uid-owner", cb->arg);
		if (cb->invert)
			info->invert |= IPT_OWNER_UID;
		info->match |= IPT_OWNER_UID;
		info->uid    = id;
		break;
	case O_GROUP:
		if ((grp = getgrnam(cb->arg)) != NULL)
			id = grp->gr_gid;
		else if (!xtables_strtoui(cb->arg, NULL, &id, 0, UINT32_MAX - 1))
			xtables_param_act(XTF_BAD_VALUE, "owner", "--gid-owner", cb->arg);
		if (cb->invert)
			info->invert |= IPT_OWNER_GID;
		info->match |= IPT_OWNER_GID;
		info->gid    = id;
		break;
	case O_PROCESS:
		if (cb->invert)
			info->invert |= IPT_OWNER_PID;
		info->match |= IPT_OWNER_PID;
		break;
	case O_SESSION:
		if (cb->invert)
			info->invert |= IPT_OWNER_SID;
		info->match |= IPT_OWNER_SID;
		break;
	case O_COMM:
		if (cb->invert)
			info->invert |= IPT_OWNER_COMM;
		info->match |= IPT_OWNER_COMM;
		break;
	}
}

static void owner_mt6_parse_v0(struct xt_option_call *cb)
{
	struct ip6t_owner_info *info = cb->data;
	struct passwd *pwd;
	struct group *grp;
	unsigned int id;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_USER:
		if ((pwd = getpwnam(cb->arg)) != NULL)
			id = pwd->pw_uid;
		else if (!xtables_strtoui(cb->arg, NULL, &id, 0, UINT32_MAX - 1))
			xtables_param_act(XTF_BAD_VALUE, "owner", "--uid-owner", cb->arg);
		if (cb->invert)
			info->invert |= IP6T_OWNER_UID;
		info->match |= IP6T_OWNER_UID;
		info->uid    = id;
		break;
	case O_GROUP:
		if ((grp = getgrnam(cb->arg)) != NULL)
			id = grp->gr_gid;
		else if (!xtables_strtoui(cb->arg, NULL, &id, 0, UINT32_MAX - 1))
			xtables_param_act(XTF_BAD_VALUE, "owner", "--gid-owner", cb->arg);
		if (cb->invert)
			info->invert |= IP6T_OWNER_GID;
		info->match |= IP6T_OWNER_GID;
		info->gid    = id;
		break;
	case O_PROCESS:
		if (cb->invert)
			info->invert |= IP6T_OWNER_PID;
		info->match |= IP6T_OWNER_PID;
		break;
	case O_SESSION:
		if (cb->invert)
			info->invert |= IP6T_OWNER_SID;
		info->match |= IP6T_OWNER_SID;
		break;
	}
}

static void owner_parse_range(const char *s, unsigned int *from,
                              unsigned int *to, const char *opt)
{
	char *end;

	/* -1 is reversed, so the max is one less than that. */
	if (!xtables_strtoui(s, &end, from, 0, UINT32_MAX - 1))
		xtables_param_act(XTF_BAD_VALUE, "owner", opt, s);
	*to = *from;
	if (*end == '-' || *end == ':')
		if (!xtables_strtoui(end + 1, &end, to, 0, UINT32_MAX - 1))
			xtables_param_act(XTF_BAD_VALUE, "owner", opt, s);
	if (*end != '\0')
		xtables_param_act(XTF_BAD_VALUE, "owner", opt, s);
}

static void owner_mt_parse(struct xt_option_call *cb)
{
	struct xt_owner_match_info *info = cb->data;
	struct passwd *pwd;
	struct group *grp;
	unsigned int from, to;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_USER:
		if ((pwd = getpwnam(cb->arg)) != NULL)
			from = to = pwd->pw_uid;
		else
			owner_parse_range(cb->arg, &from, &to, "--uid-owner");
		if (cb->invert)
			info->invert |= XT_OWNER_UID;
		info->match  |= XT_OWNER_UID;
		info->uid_min = from;
		info->uid_max = to;
		break;
	case O_GROUP:
		if ((grp = getgrnam(cb->arg)) != NULL)
			from = to = grp->gr_gid;
		else
			owner_parse_range(cb->arg, &from, &to, "--gid-owner");
		if (cb->invert)
			info->invert |= XT_OWNER_GID;
		info->match  |= XT_OWNER_GID;
		info->gid_min = from;
		info->gid_max = to;
		break;
	case O_SOCK_EXISTS:
		if (cb->invert)
			info->invert |= XT_OWNER_SOCKET;
		info->match |= XT_OWNER_SOCKET;
		break;
	}
}

static void owner_mt_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM, "owner: At least one of "
		           "--uid-owner, --gid-owner or --socket-exists "
		           "is required");
}

static void
owner_mt_print_item_v0(const struct ipt_owner_info *info, const char *label,
                       uint8_t flag, bool numeric)
{
	if (!(info->match & flag))
		return;
	if (info->invert & flag)
		printf(" !");
	printf(" %s", label);

	switch (info->match & flag) {
	case IPT_OWNER_UID:
		if (!numeric) {
			struct passwd *pwd = getpwuid(info->uid);

			if (pwd != NULL && pwd->pw_name != NULL) {
				printf(" %s", pwd->pw_name);
				break;
			}
		}
		printf(" %u", (unsigned int)info->uid);
		break;

	case IPT_OWNER_GID:
		if (!numeric) {
			struct group *grp = getgrgid(info->gid);

			if (grp != NULL && grp->gr_name != NULL) {
				printf(" %s", grp->gr_name);
				break;
			}
		}
		printf(" %u", (unsigned int)info->gid);
		break;

	case IPT_OWNER_PID:
		printf(" %u", (unsigned int)info->pid);
		break;

	case IPT_OWNER_SID:
		printf(" %u", (unsigned int)info->sid);
		break;

	case IPT_OWNER_COMM:
		printf(" %.*s", (int)sizeof(info->comm), info->comm);
		break;
	}
}

static void
owner_mt6_print_item_v0(const struct ip6t_owner_info *info, const char *label,
                        uint8_t flag, bool numeric)
{
	if (!(info->match & flag))
		return;
	if (info->invert & flag)
		printf(" !");
	printf(" %s", label);

	switch (info->match & flag) {
	case IP6T_OWNER_UID:
		if (!numeric) {
			struct passwd *pwd = getpwuid(info->uid);

			if (pwd != NULL && pwd->pw_name != NULL) {
				printf(" %s", pwd->pw_name);
				break;
			}
		}
		printf(" %u", (unsigned int)info->uid);
		break;

	case IP6T_OWNER_GID:
		if (!numeric) {
			struct group *grp = getgrgid(info->gid);

			if (grp != NULL && grp->gr_name != NULL) {
				printf(" %s", grp->gr_name);
				break;
			}
		}
		printf(" %u", (unsigned int)info->gid);
		break;

	case IP6T_OWNER_PID:
		printf(" %u", (unsigned int)info->pid);
		break;

	case IP6T_OWNER_SID:
		printf(" %u", (unsigned int)info->sid);
		break;
	}
}

static void
owner_mt_print_item(const struct xt_owner_match_info *info, const char *label,
                    uint8_t flag, bool numeric)
{
	if (!(info->match & flag))
		return;
	if (info->invert & flag)
		printf(" !");
	printf(" %s", label);

	switch (info->match & flag) {
	case XT_OWNER_UID:
		if (info->uid_min != info->uid_max) {
			printf(" %u-%u", (unsigned int)info->uid_min,
			       (unsigned int)info->uid_max);
			break;
		} else if (!numeric) {
			const struct passwd *pwd = getpwuid(info->uid_min);

			if (pwd != NULL && pwd->pw_name != NULL) {
				printf(" %s", pwd->pw_name);
				break;
			}
		}
		printf(" %u", (unsigned int)info->uid_min);
		break;

	case XT_OWNER_GID:
		if (info->gid_min != info->gid_max) {
			printf(" %u-%u", (unsigned int)info->gid_min,
			       (unsigned int)info->gid_max);
			break;
		} else if (!numeric) {
			const struct group *grp = getgrgid(info->gid_min);

			if (grp != NULL && grp->gr_name != NULL) {
				printf(" %s", grp->gr_name);
				break;
			}
		}
		printf(" %u", (unsigned int)info->gid_min);
		break;
	}
}

static void
owner_mt_print_v0(const void *ip, const struct xt_entry_match *match,
                  int numeric)
{
	const struct ipt_owner_info *info = (void *)match->data;

	owner_mt_print_item_v0(info, "owner UID match", IPT_OWNER_UID, numeric);
	owner_mt_print_item_v0(info, "owner GID match", IPT_OWNER_GID, numeric);
	owner_mt_print_item_v0(info, "owner PID match", IPT_OWNER_PID, numeric);
	owner_mt_print_item_v0(info, "owner SID match", IPT_OWNER_SID, numeric);
	owner_mt_print_item_v0(info, "owner CMD match", IPT_OWNER_COMM, numeric);
}

static void
owner_mt6_print_v0(const void *ip, const struct xt_entry_match *match,
                   int numeric)
{
	const struct ip6t_owner_info *info = (void *)match->data;

	owner_mt6_print_item_v0(info, "owner UID match", IPT_OWNER_UID, numeric);
	owner_mt6_print_item_v0(info, "owner GID match", IPT_OWNER_GID, numeric);
	owner_mt6_print_item_v0(info, "owner PID match", IPT_OWNER_PID, numeric);
	owner_mt6_print_item_v0(info, "owner SID match", IPT_OWNER_SID, numeric);
}

static void owner_mt_print(const void *ip, const struct xt_entry_match *match,
                           int numeric)
{
	const struct xt_owner_match_info *info = (void *)match->data;

	owner_mt_print_item(info, "owner socket exists", XT_OWNER_SOCKET, numeric);
	owner_mt_print_item(info, "owner UID match",     XT_OWNER_UID,    numeric);
	owner_mt_print_item(info, "owner GID match",     XT_OWNER_GID,    numeric);
}

static void
owner_mt_save_v0(const void *ip, const struct xt_entry_match *match)
{
	const struct ipt_owner_info *info = (void *)match->data;

	owner_mt_print_item_v0(info, "--uid-owner", IPT_OWNER_UID, true);
	owner_mt_print_item_v0(info, "--gid-owner", IPT_OWNER_GID, true);
	owner_mt_print_item_v0(info, "--pid-owner", IPT_OWNER_PID, true);
	owner_mt_print_item_v0(info, "--sid-owner", IPT_OWNER_SID, true);
	owner_mt_print_item_v0(info, "--cmd-owner", IPT_OWNER_COMM, true);
}

static void
owner_mt6_save_v0(const void *ip, const struct xt_entry_match *match)
{
	const struct ip6t_owner_info *info = (void *)match->data;

	owner_mt6_print_item_v0(info, "--uid-owner", IPT_OWNER_UID, true);
	owner_mt6_print_item_v0(info, "--gid-owner", IPT_OWNER_GID, true);
	owner_mt6_print_item_v0(info, "--pid-owner", IPT_OWNER_PID, true);
	owner_mt6_print_item_v0(info, "--sid-owner", IPT_OWNER_SID, true);
}

static void owner_mt_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_owner_match_info *info = (void *)match->data;

	owner_mt_print_item(info, "--socket-exists",  XT_OWNER_SOCKET, true);
	owner_mt_print_item(info, "--uid-owner",      XT_OWNER_UID,    true);
	owner_mt_print_item(info, "--gid-owner",      XT_OWNER_GID,    true);
}

static struct xtables_match owner_mt_reg[] = {
	{
		.version       = XTABLES_VERSION,
		.name          = "owner",
		.revision      = 0,
		.family        = NFPROTO_IPV4,
		.size          = XT_ALIGN(sizeof(struct ipt_owner_info)),
		.userspacesize = XT_ALIGN(sizeof(struct ipt_owner_info)),
		.help          = owner_mt_help_v0,
		.x6_parse      = owner_mt_parse_v0,
		.x6_fcheck     = owner_mt_check,
		.print         = owner_mt_print_v0,
		.save          = owner_mt_save_v0,
		.x6_options    = owner_mt_opts_v0,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "owner",
		.revision      = 0,
		.family        = NFPROTO_IPV6,
		.size          = XT_ALIGN(sizeof(struct ip6t_owner_info)),
		.userspacesize = XT_ALIGN(sizeof(struct ip6t_owner_info)),
		.help          = owner_mt6_help_v0,
		.x6_parse      = owner_mt6_parse_v0,
		.x6_fcheck     = owner_mt_check,
		.print         = owner_mt6_print_v0,
		.save          = owner_mt6_save_v0,
		.x6_options    = owner_mt6_opts_v0,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "owner",
		.revision      = 1,
		.family        = NFPROTO_UNSPEC,
		.size          = XT_ALIGN(sizeof(struct xt_owner_match_info)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_owner_match_info)),
		.help          = owner_mt_help,
		.x6_parse      = owner_mt_parse,
		.x6_fcheck     = owner_mt_check,
		.print         = owner_mt_print,
		.save          = owner_mt_save,
		.x6_options    = owner_mt_opts,
	},
};

void _init(void)
{
	xtables_register_matches(owner_mt_reg, ARRAY_SIZE(owner_mt_reg));
}
