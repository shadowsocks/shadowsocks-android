#ifndef _XT_SET_H
#define _XT_SET_H

/* The protocol version */
#define IPSET_PROTOCOL		5

/* The max length of strings including NUL: set and type identifiers */
#define IPSET_MAXNAMELEN	32

/* Sets are identified by an index in kernel space. Tweak with ip_set_id_t
 * and IPSET_INVALID_ID if you want to increase the max number of sets.
 */
typedef uint16_t ip_set_id_t;

#define IPSET_INVALID_ID	65535

enum ip_set_dim {
	IPSET_DIM_ZERO = 0,
	IPSET_DIM_ONE,
	IPSET_DIM_TWO,
	IPSET_DIM_THREE,
	/* Max dimension in elements.
	 * If changed, new revision of iptables match/target is required.
	 */
	IPSET_DIM_MAX = 6,
};

/* Option flags for kernel operations */
enum ip_set_kopt {
	IPSET_INV_MATCH = (1 << IPSET_DIM_ZERO),
	IPSET_DIM_ONE_SRC = (1 << IPSET_DIM_ONE),
	IPSET_DIM_TWO_SRC = (1 << IPSET_DIM_TWO),
	IPSET_DIM_THREE_SRC = (1 << IPSET_DIM_THREE),
};

/* Interface to iptables/ip6tables */

#define SO_IP_SET 		83

union ip_set_name_index {
	char name[IPSET_MAXNAMELEN];
	ip_set_id_t index;
};

#define IP_SET_OP_GET_BYNAME	0x00000006	/* Get set index by name */
struct ip_set_req_get_set {
	unsigned op;
	unsigned version;
	union ip_set_name_index set;
};

#define IP_SET_OP_GET_BYINDEX	0x00000007	/* Get set name by index */
/* Uses ip_set_req_get_set */

#define IP_SET_OP_VERSION	0x00000100	/* Ask kernel version */
struct ip_set_req_version {
	unsigned op;
	unsigned version;
};

/* Revision 0 interface: backward compatible with netfilter/iptables */

/*
 * Option flags for kernel operations (xt_set_info_v0)
 */
#define IPSET_SRC		0x01	/* Source match/add */
#define IPSET_DST		0x02	/* Destination match/add */
#define IPSET_MATCH_INV		0x04	/* Inverse matching */

struct xt_set_info_v0 {
	ip_set_id_t index;
	union {
		u_int32_t flags[IPSET_DIM_MAX + 1];
		struct {
			u_int32_t __flags[IPSET_DIM_MAX];
			u_int8_t dim;
			u_int8_t flags;
		} compat;
	} u;
};

/* match and target infos */
struct xt_set_info_match_v0 {
	struct xt_set_info_v0 match_set;
};

struct xt_set_info_target_v0 {
	struct xt_set_info_v0 add_set;
	struct xt_set_info_v0 del_set;
};

/* Revision 1 match and target */

struct xt_set_info {
	ip_set_id_t index;
	u_int8_t dim;
	u_int8_t flags;
};

/* match and target infos */
struct xt_set_info_match_v1 {
	struct xt_set_info match_set;
};

struct xt_set_info_target_v1 {
	struct xt_set_info add_set;
	struct xt_set_info del_set;
};

/* Revision 2 target */

enum ipset_cmd_flags {
	IPSET_FLAG_BIT_EXIST	= 0,
	IPSET_FLAG_EXIST	= (1 << IPSET_FLAG_BIT_EXIST),
};

struct xt_set_info_target_v2 {
	struct xt_set_info add_set;
	struct xt_set_info del_set;
	u_int32_t flags;
	u_int32_t timeout;
};

#endif /*_XT_SET_H*/
