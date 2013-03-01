/* Library which manipulates firewall rules.  Version 0.1. */

/* Architecture of firewall rules is as follows:
 *
 * Chains go INPUT, FORWARD, OUTPUT then user chains.
 * Each user chain starts with an ERROR node.
 * Every chain ends with an unconditional jump: a RETURN for user chains,
 * and a POLICY for built-ins.
 */

/* (C)1999 Paul ``Rusty'' Russell - Placed under the GNU GPL (See
   COPYING for details). */

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

#ifdef DEBUG_CONNTRACK
#define inline
#endif

#if !defined(__ANDROID__) && (!defined(__GLIBC__) || (__GLIBC__ < 2))
typedef unsigned int socklen_t;
#endif

#include "libiptc/libip6tc.h"

#define HOOK_PRE_ROUTING	NF_IP6_PRE_ROUTING
#define HOOK_LOCAL_IN		NF_IP6_LOCAL_IN
#define HOOK_FORWARD		NF_IP6_FORWARD
#define HOOK_LOCAL_OUT		NF_IP6_LOCAL_OUT
#define HOOK_POST_ROUTING	NF_IP6_POST_ROUTING

#define STRUCT_ENTRY_TARGET	struct ip6t_entry_target
#define STRUCT_ENTRY		struct ip6t_entry
#define STRUCT_ENTRY_MATCH	struct ip6t_entry_match
#define STRUCT_GETINFO		struct ip6t_getinfo
#define STRUCT_GET_ENTRIES	struct ip6t_get_entries
#define STRUCT_COUNTERS		struct ip6t_counters
#define STRUCT_COUNTERS_INFO	struct ip6t_counters_info
#define STRUCT_STANDARD_TARGET	struct ip6t_standard_target
#define STRUCT_REPLACE		struct ip6t_replace

#define STRUCT_TC_HANDLE	struct ip6tc_handle
#define xtc_handle		ip6tc_handle

#define ENTRY_ITERATE		IP6T_ENTRY_ITERATE
#define TABLE_MAXNAMELEN	IP6T_TABLE_MAXNAMELEN
#define FUNCTION_MAXNAMELEN	IP6T_FUNCTION_MAXNAMELEN

#define GET_TARGET		ip6t_get_target

#define ERROR_TARGET		IP6T_ERROR_TARGET
#define NUMHOOKS		NF_IP6_NUMHOOKS

#define IPT_CHAINLABEL		ip6t_chainlabel

#define TC_DUMP_ENTRIES		dump_entries6
#define TC_IS_CHAIN		ip6tc_is_chain
#define TC_FIRST_CHAIN		ip6tc_first_chain
#define TC_NEXT_CHAIN		ip6tc_next_chain
#define TC_FIRST_RULE		ip6tc_first_rule
#define TC_NEXT_RULE		ip6tc_next_rule
#define TC_GET_TARGET		ip6tc_get_target
#define TC_BUILTIN		ip6tc_builtin
#define TC_GET_POLICY		ip6tc_get_policy
#define TC_INSERT_ENTRY		ip6tc_insert_entry
#define TC_REPLACE_ENTRY	ip6tc_replace_entry
#define TC_APPEND_ENTRY		ip6tc_append_entry
#define TC_CHECK_ENTRY		ip6tc_check_entry
#define TC_DELETE_ENTRY		ip6tc_delete_entry
#define TC_DELETE_NUM_ENTRY	ip6tc_delete_num_entry
#define TC_FLUSH_ENTRIES	ip6tc_flush_entries
#define TC_ZERO_ENTRIES		ip6tc_zero_entries
#define TC_ZERO_COUNTER		ip6tc_zero_counter
#define TC_READ_COUNTER		ip6tc_read_counter
#define TC_SET_COUNTER		ip6tc_set_counter
#define TC_CREATE_CHAIN		ip6tc_create_chain
#define TC_GET_REFERENCES	ip6tc_get_references
#define TC_DELETE_CHAIN		ip6tc_delete_chain
#define TC_RENAME_CHAIN		ip6tc_rename_chain
#define TC_SET_POLICY		ip6tc_set_policy
#define TC_GET_RAW_SOCKET	ip6tc_get_raw_socket
#define TC_INIT			ip6tc_init
#define TC_FREE			ip6tc_free
#define TC_COMMIT		ip6tc_commit
#define TC_STRERROR		ip6tc_strerror
#define TC_NUM_RULES		ip6tc_num_rules
#define TC_GET_RULE		ip6tc_get_rule

#define TC_AF			AF_INET6
#define TC_IPPROTO		IPPROTO_IPV6

#define SO_SET_REPLACE		IP6T_SO_SET_REPLACE
#define SO_SET_ADD_COUNTERS	IP6T_SO_SET_ADD_COUNTERS
#define SO_GET_INFO		IP6T_SO_GET_INFO
#define SO_GET_ENTRIES		IP6T_SO_GET_ENTRIES
#define SO_GET_VERSION		IP6T_SO_GET_VERSION

#define STANDARD_TARGET		IP6T_STANDARD_TARGET
#define LABEL_RETURN		IP6TC_LABEL_RETURN
#define LABEL_ACCEPT		IP6TC_LABEL_ACCEPT
#define LABEL_DROP		IP6TC_LABEL_DROP
#define LABEL_QUEUE		IP6TC_LABEL_QUEUE

#define ALIGN			XT_ALIGN
#define RETURN			IP6T_RETURN

#include "libiptc.c"

#define BIT6(a, l) \
 ((ntohl(a->s6_addr32[(l) / 32]) >> (31 - ((l) & 31))) & 1)

int
ipv6_prefix_length(const struct in6_addr *a)
{
	int l, i;
	for (l = 0; l < 128; l++) {
		if (BIT6(a, l) == 0)
			break;
	}
	for (i = l + 1; i < 128; i++) {
		if (BIT6(a, i) == 1)
			return -1;
	}
	return l;
}

static int
dump_entry(struct ip6t_entry *e, struct ip6tc_handle *const handle)
{
	size_t i;
	char buf[40];
	int len;
	struct ip6t_entry_target *t;
	
	printf("Entry %u (%lu):\n", iptcb_entry2index(handle, e),
	       iptcb_entry2offset(handle, e));
	puts("SRC IP: ");
	inet_ntop(AF_INET6, &e->ipv6.src, buf, sizeof buf);
	puts(buf);
	putchar('/');
	len = ipv6_prefix_length(&e->ipv6.smsk);
	if (len != -1)
		printf("%d", len);
	else {
		inet_ntop(AF_INET6, &e->ipv6.smsk, buf, sizeof buf);
		puts(buf);
	}
	putchar('\n');
	
	puts("DST IP: ");
	inet_ntop(AF_INET6, &e->ipv6.dst, buf, sizeof buf);
	puts(buf);
	putchar('/');
	len = ipv6_prefix_length(&e->ipv6.dmsk);
	if (len != -1)
		printf("%d", len);
	else {
		inet_ntop(AF_INET6, &e->ipv6.dmsk, buf, sizeof buf);
		puts(buf);
	}
	putchar('\n');
	
	printf("Interface: `%s'/", e->ipv6.iniface);
	for (i = 0; i < IFNAMSIZ; i++)
		printf("%c", e->ipv6.iniface_mask[i] ? 'X' : '.');
	printf("to `%s'/", e->ipv6.outiface);
	for (i = 0; i < IFNAMSIZ; i++)
		printf("%c", e->ipv6.outiface_mask[i] ? 'X' : '.');
	printf("\nProtocol: %u\n", e->ipv6.proto);
	if (e->ipv6.flags & IP6T_F_TOS)
		printf("TOS: %u\n", e->ipv6.tos);
	printf("Flags: %02X\n", e->ipv6.flags);
	printf("Invflags: %02X\n", e->ipv6.invflags);
	printf("Counters: %llu packets, %llu bytes\n",
	       (unsigned long long)e->counters.pcnt, (unsigned long long)e->counters.bcnt);
	printf("Cache: %08X\n", e->nfcache);
	
	IP6T_MATCH_ITERATE(e, print_match);

	t = ip6t_get_target(e);
	printf("Target name: `%s' [%u]\n", t->u.user.name, t->u.target_size);
	if (strcmp(t->u.user.name, IP6T_STANDARD_TARGET) == 0) {
		const unsigned char *data = t->data;
		int pos = *(const int *)data;
		if (pos < 0)
			printf("verdict=%s\n",
			       pos == -NF_ACCEPT-1 ? "NF_ACCEPT"
			       : pos == -NF_DROP-1 ? "NF_DROP"
			       : pos == IP6T_RETURN ? "RETURN"
			       : "UNKNOWN");
		else
			printf("verdict=%u\n", pos);
	} else if (strcmp(t->u.user.name, IP6T_ERROR_TARGET) == 0)
		printf("error=`%s'\n", t->data);

	printf("\n");
	return 0;
}

static unsigned char *
is_same(const STRUCT_ENTRY *a, const STRUCT_ENTRY *b,
	unsigned char *matchmask)
{
	unsigned int i;
	unsigned char *mptr;

	/* Always compare head structures: ignore mask here. */
	if (memcmp(&a->ipv6.src, &b->ipv6.src, sizeof(struct in6_addr))
	    || memcmp(&a->ipv6.dst, &b->ipv6.dst, sizeof(struct in6_addr))
	    || memcmp(&a->ipv6.smsk, &b->ipv6.smsk, sizeof(struct in6_addr))
	    || memcmp(&a->ipv6.dmsk, &b->ipv6.dmsk, sizeof(struct in6_addr))
	    || a->ipv6.proto != b->ipv6.proto
	    || a->ipv6.tos != b->ipv6.tos
	    || a->ipv6.flags != b->ipv6.flags
	    || a->ipv6.invflags != b->ipv6.invflags)
		return NULL;

	for (i = 0; i < IFNAMSIZ; i++) {
		if (a->ipv6.iniface_mask[i] != b->ipv6.iniface_mask[i])
			return NULL;
		if ((a->ipv6.iniface[i] & a->ipv6.iniface_mask[i])
		    != (b->ipv6.iniface[i] & b->ipv6.iniface_mask[i]))
			return NULL;
		if (a->ipv6.outiface_mask[i] != b->ipv6.outiface_mask[i])
			return NULL;
		if ((a->ipv6.outiface[i] & a->ipv6.outiface_mask[i])
		    != (b->ipv6.outiface[i] & b->ipv6.outiface_mask[i]))
			return NULL;
	}

	if (a->target_offset != b->target_offset
	    || a->next_offset != b->next_offset)
		return NULL;

	mptr = matchmask + sizeof(STRUCT_ENTRY);
	if (IP6T_MATCH_ITERATE(a, match_different, a->elems, b->elems, &mptr))
		return NULL;
	mptr += XT_ALIGN(sizeof(struct ip6t_entry_target));

	return mptr;
}

/* All zeroes == unconditional rule. */
static inline int
unconditional(const struct ip6t_ip6 *ipv6)
{
	unsigned int i;

	for (i = 0; i < sizeof(*ipv6); i++)
		if (((char *)ipv6)[i])
			break;

	return (i == sizeof(*ipv6));
}

#ifdef IPTC_DEBUG
/* Do every conceivable sanity check on the handle */
static void
do_check(struct xtc_handle *h, unsigned int line)
{
	unsigned int i, n;
	unsigned int user_offset; /* Offset of first user chain */
	int was_return;

	assert(h->changed == 0 || h->changed == 1);
	if (strcmp(h->info.name, "filter") == 0) {
		assert(h->info.valid_hooks
		       == (1 << NF_IP6_LOCAL_IN
			   | 1 << NF_IP6_FORWARD
			   | 1 << NF_IP6_LOCAL_OUT));

		/* Hooks should be first three */
		assert(h->info.hook_entry[NF_IP6_LOCAL_IN] == 0);

		n = get_chain_end(h, 0);
		n += get_entry(h, n)->next_offset;
		assert(h->info.hook_entry[NF_IP6_FORWARD] == n);

		n = get_chain_end(h, n);
		n += get_entry(h, n)->next_offset;
		assert(h->info.hook_entry[NF_IP6_LOCAL_OUT] == n);

		user_offset = h->info.hook_entry[NF_IP6_LOCAL_OUT];
	} else if (strcmp(h->info.name, "nat") == 0) {
		assert((h->info.valid_hooks
			== (1 << NF_IP6_PRE_ROUTING
			    | 1 << NF_IP6_LOCAL_OUT
			    | 1 << NF_IP6_POST_ROUTING)) ||
		       (h->info.valid_hooks
			== (1 << NF_IP6_PRE_ROUTING
			    | 1 << NF_IP6_LOCAL_IN
			    | 1 << NF_IP6_LOCAL_OUT
			    | 1 << NF_IP6_POST_ROUTING)));

		assert(h->info.hook_entry[NF_IP6_PRE_ROUTING] == 0);

		n = get_chain_end(h, 0);

		n += get_entry(h, n)->next_offset;
		assert(h->info.hook_entry[NF_IP6_POST_ROUTING] == n);
		n = get_chain_end(h, n);

		n += get_entry(h, n)->next_offset;
		assert(h->info.hook_entry[NF_IP6_LOCAL_OUT] == n);
		user_offset = h->info.hook_entry[NF_IP6_LOCAL_OUT];

		if (h->info.valid_hooks & (1 << NF_IP6_LOCAL_IN)) {
			n = get_chain_end(h, n);
			n += get_entry(h, n)->next_offset;
			assert(h->info.hook_entry[NF_IP6_LOCAL_IN] == n);
			user_offset = h->info.hook_entry[NF_IP6_LOCAL_IN];
		}

	} else if (strcmp(h->info.name, "mangle") == 0) {
		/* This code is getting ugly because linux < 2.4.18-pre6 had
		 * two mangle hooks, linux >= 2.4.18-pre6 has five mangle hooks
		 * */
		assert((h->info.valid_hooks
			== (1 << NF_IP6_PRE_ROUTING
			    | 1 << NF_IP6_LOCAL_OUT)) ||
		       (h->info.valid_hooks
			== (1 << NF_IP6_PRE_ROUTING
			    | 1 << NF_IP6_LOCAL_IN
			    | 1 << NF_IP6_FORWARD
			    | 1 << NF_IP6_LOCAL_OUT
			    | 1 << NF_IP6_POST_ROUTING)));

		/* Hooks should be first five */
		assert(h->info.hook_entry[NF_IP6_PRE_ROUTING] == 0);

		n = get_chain_end(h, 0);

		if (h->info.valid_hooks & (1 << NF_IP6_LOCAL_IN)) {
			n += get_entry(h, n)->next_offset;
			assert(h->info.hook_entry[NF_IP6_LOCAL_IN] == n);
			n = get_chain_end(h, n);
		}

		if (h->info.valid_hooks & (1 << NF_IP6_FORWARD)) {
			n += get_entry(h, n)->next_offset;
			assert(h->info.hook_entry[NF_IP6_FORWARD] == n);
			n = get_chain_end(h, n);
		}

		n += get_entry(h, n)->next_offset;
		assert(h->info.hook_entry[NF_IP6_LOCAL_OUT] == n);
		user_offset = h->info.hook_entry[NF_IP6_LOCAL_OUT];

		if (h->info.valid_hooks & (1 << NF_IP6_POST_ROUTING)) {
			n = get_chain_end(h, n);
			n += get_entry(h, n)->next_offset;
			assert(h->info.hook_entry[NF_IP6_POST_ROUTING] == n);
			user_offset = h->info.hook_entry[NF_IP6_POST_ROUTING];
		}
	} else if (strcmp(h->info.name, "raw") == 0) {
		assert(h->info.valid_hooks
		       == (1 << NF_IP6_PRE_ROUTING
			   | 1 << NF_IP6_LOCAL_OUT));

		/* Hooks should be first three */
		assert(h->info.hook_entry[NF_IP6_PRE_ROUTING] == 0);

		n = get_chain_end(h, n);
		n += get_entry(h, n)->next_offset;
		assert(h->info.hook_entry[NF_IP6_LOCAL_OUT] == n);

		user_offset = h->info.hook_entry[NF_IP6_LOCAL_OUT];
	} else {
                fprintf(stderr, "Unknown table `%s'\n", h->info.name);
		abort();
	}

	/* User chain == end of last builtin + policy entry */
	user_offset = get_chain_end(h, user_offset);
	user_offset += get_entry(h, user_offset)->next_offset;

	/* Overflows should be end of entry chains, and unconditional
           policy nodes. */
	for (i = 0; i < NUMHOOKS; i++) {
		STRUCT_ENTRY *e;
		STRUCT_STANDARD_TARGET *t;

		if (!(h->info.valid_hooks & (1 << i)))
			continue;
		assert(h->info.underflow[i]
		       == get_chain_end(h, h->info.hook_entry[i]));

		e = get_entry(h, get_chain_end(h, h->info.hook_entry[i]));
		assert(unconditional(&e->ipv6));
		assert(e->target_offset == sizeof(*e));
		t = (STRUCT_STANDARD_TARGET *)GET_TARGET(e);
		printf("target_size=%u, align=%u\n",
			t->target.u.target_size, ALIGN(sizeof(*t)));
		assert(t->target.u.target_size == ALIGN(sizeof(*t)));
		assert(e->next_offset == sizeof(*e) + ALIGN(sizeof(*t)));

		assert(strcmp(t->target.u.user.name, STANDARD_TARGET)==0);
		assert(t->verdict == -NF_DROP-1 || t->verdict == -NF_ACCEPT-1);

		/* Hooks and underflows must be valid entries */
		iptcb_entry2index(h, get_entry(h, h->info.hook_entry[i]));
		iptcb_entry2index(h, get_entry(h, h->info.underflow[i]));
	}

	assert(h->info.size
	       >= h->info.num_entries * (sizeof(STRUCT_ENTRY)
					 +sizeof(STRUCT_STANDARD_TARGET)));

	assert(h->entries.size
	       >= (h->new_number
		   * (sizeof(STRUCT_ENTRY)
		      + sizeof(STRUCT_STANDARD_TARGET))));
	assert(strcmp(h->info.name, h->entries.name) == 0);

	i = 0; n = 0;
	was_return = 0;

#if 0
	/* Check all the entries. */
	ENTRY_ITERATE(h->entries.entrytable, h->entries.size,
		      check_entry, &i, &n, user_offset, &was_return, h);

	assert(i == h->new_number);
	assert(n == h->entries.size);

	/* Final entry must be error node */
	assert(strcmp(GET_TARGET(index2entry(h, h->new_number-1))
		      ->u.user.name,
		      ERROR_TARGET) == 0);
#endif
}
#endif /*IPTC_DEBUG*/
