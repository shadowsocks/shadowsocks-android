/* Shared library add-on to iptables to add CLUSTERIP target support. 
 * (C) 2003 by Harald Welte <laforge@gnumonks.org>
 *
 * Development of this code was funded by SuSE AG, http://www.suse.com/
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stddef.h>

#if defined(__GLIBC__) && __GLIBC__ == 2
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif

#include <xtables.h>
#include <linux/netfilter_ipv4/ipt_CLUSTERIP.h>

enum {
	O_NEW = 0,
	O_HASHMODE,
	O_CLUSTERMAC,
	O_TOTAL_NODES,
	O_LOCAL_NODE,
	O_HASH_INIT,
	F_NEW         = 1 << O_NEW,
	F_HASHMODE    = 1 << O_HASHMODE,
	F_CLUSTERMAC  = 1 << O_CLUSTERMAC,
	F_TOTAL_NODES = 1 << O_TOTAL_NODES,
	F_LOCAL_NODE  = 1 << O_LOCAL_NODE,
	F_FULL        = F_NEW | F_HASHMODE | F_CLUSTERMAC |
	                F_TOTAL_NODES | F_LOCAL_NODE,
};

static void CLUSTERIP_help(void)
{
	printf(
"CLUSTERIP target options:\n"
"  --new			 Create a new ClusterIP\n"
"  --hashmode <mode>		 Specify hashing mode\n"
"					sourceip\n"
"					sourceip-sourceport\n"
"					sourceip-sourceport-destport\n"
"  --clustermac <mac>		 Set clusterIP MAC address\n"
"  --total-nodes <num>		 Set number of total nodes in cluster\n"
"  --local-node <num>		 Set the local node number\n"
"  --hash-init <num>		 Set init value of the Jenkins hash\n");
}

#define s struct ipt_clusterip_tgt_info
static const struct xt_option_entry CLUSTERIP_opts[] = {
	{.name = "new", .id = O_NEW, .type = XTTYPE_NONE},
	{.name = "hashmode", .id = O_HASHMODE, .type = XTTYPE_STRING,
	 .also = O_NEW},
	{.name = "clustermac", .id = O_CLUSTERMAC, .type = XTTYPE_ETHERMAC,
	 .also = O_NEW, .flags = XTOPT_PUT, XTOPT_POINTER(s, clustermac)},
	{.name = "total-nodes", .id = O_TOTAL_NODES, .type = XTTYPE_UINT16,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, num_total_nodes),
	 .also = O_NEW, .max = CLUSTERIP_MAX_NODES},
	{.name = "local-node", .id = O_LOCAL_NODE, .type = XTTYPE_UINT16,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, local_nodes[0]),
	 .also = O_NEW, .max = CLUSTERIP_MAX_NODES},
	{.name = "hash-init", .id = O_HASH_INIT, .type = XTTYPE_UINT32,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, hash_initval),
	 .also = O_NEW, .max = UINT_MAX},
	XTOPT_TABLEEND,
};
#undef s

static void CLUSTERIP_parse(struct xt_option_call *cb)
{
	struct ipt_clusterip_tgt_info *cipinfo = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_NEW:
		cipinfo->flags |= CLUSTERIP_FLAG_NEW;
		break;
	case O_HASHMODE:
		if (strcmp(cb->arg, "sourceip") == 0)
			cipinfo->hash_mode = CLUSTERIP_HASHMODE_SIP;
		else if (strcmp(cb->arg, "sourceip-sourceport") == 0)
			cipinfo->hash_mode = CLUSTERIP_HASHMODE_SIP_SPT;
		else if (strcmp(cb->arg, "sourceip-sourceport-destport") == 0)
			cipinfo->hash_mode = CLUSTERIP_HASHMODE_SIP_SPT_DPT;
		else
			xtables_error(PARAMETER_PROBLEM, "Unknown hashmode \"%s\"\n",
				   cb->arg);
		break;
	case O_CLUSTERMAC:
		if (!(cipinfo->clustermac[0] & 0x01))
			xtables_error(PARAMETER_PROBLEM, "MAC has to be a multicast ethernet address\n");
		break;
	case O_LOCAL_NODE:
		cipinfo->num_local_nodes = 1;
		break;
	}
}

static void CLUSTERIP_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		return;
	if ((cb->xflags & F_FULL) == F_FULL)
		return;

	xtables_error(PARAMETER_PROBLEM, "CLUSTERIP target: Invalid parameter combination\n");
}

static const char *hashmode2str(enum clusterip_hashmode mode)
{
	const char *retstr;
	switch (mode) {
		case CLUSTERIP_HASHMODE_SIP:
			retstr = "sourceip";
			break;
		case CLUSTERIP_HASHMODE_SIP_SPT:
			retstr = "sourceip-sourceport";
			break;
		case CLUSTERIP_HASHMODE_SIP_SPT_DPT:
			retstr = "sourceip-sourceport-destport";
			break;
		default:
			retstr = "unknown-error";
			break;
	}
	return retstr;
}

static const char *mac2str(const uint8_t mac[ETH_ALEN])
{
	static char buf[ETH_ALEN*3];
	sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return buf;
}

static void CLUSTERIP_print(const void *ip,
                            const struct xt_entry_target *target, int numeric)
{
	const struct ipt_clusterip_tgt_info *cipinfo =
		(const struct ipt_clusterip_tgt_info *)target->data;
	
	if (!cipinfo->flags & CLUSTERIP_FLAG_NEW) {
		printf(" CLUSTERIP");
		return;
	}

	printf(" CLUSTERIP hashmode=%s clustermac=%s total_nodes=%u local_node=%u hash_init=%u",
		hashmode2str(cipinfo->hash_mode),
		mac2str(cipinfo->clustermac),
		cipinfo->num_total_nodes,
		cipinfo->local_nodes[0],
		cipinfo->hash_initval);
}

static void CLUSTERIP_save(const void *ip, const struct xt_entry_target *target)
{
	const struct ipt_clusterip_tgt_info *cipinfo =
		(const struct ipt_clusterip_tgt_info *)target->data;

	/* if this is not a new entry, we don't need to save target
	 * parameters */
	if (!cipinfo->flags & CLUSTERIP_FLAG_NEW)
		return;

	printf(" --new --hashmode %s --clustermac %s --total-nodes %d --local-node %d --hash-init %u",
	       hashmode2str(cipinfo->hash_mode),
	       mac2str(cipinfo->clustermac),
	       cipinfo->num_total_nodes,
	       cipinfo->local_nodes[0],
	       cipinfo->hash_initval);
}

static struct xtables_target clusterip_tg_reg = {
	.name		= "CLUSTERIP",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_IPV4,
	.size		= XT_ALIGN(sizeof(struct ipt_clusterip_tgt_info)),
	.userspacesize	= offsetof(struct ipt_clusterip_tgt_info, config),
 	.help		= CLUSTERIP_help,
	.x6_parse	= CLUSTERIP_parse,
	.x6_fcheck	= CLUSTERIP_check,
	.print		= CLUSTERIP_print,
	.save		= CLUSTERIP_save,
	.x6_options	= CLUSTERIP_opts,
};

void _init(void)
{
	xtables_register_target(&clusterip_tg_reg);
}
