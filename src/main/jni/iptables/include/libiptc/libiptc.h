#ifndef _LIBIPTC_H
#define _LIBIPTC_H
/* Library which manipulates filtering rules. */

#include <linux/types.h>
#include <libiptc/ipt_kernel_headers.h>
#ifdef __cplusplus
#	include <climits>
#else
#	include <limits.h> /* INT_MAX in ip_tables.h */
#endif
#include <linux/netfilter_ipv4/ip_tables.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iptc_handle;

typedef char ipt_chainlabel[32];

#define IPTC_LABEL_ACCEPT  "ACCEPT"
#define IPTC_LABEL_DROP    "DROP"
#define IPTC_LABEL_QUEUE   "QUEUE"
#define IPTC_LABEL_RETURN  "RETURN"

/* Does this chain exist? */
int iptc_is_chain(const char *chain, struct iptc_handle *const handle);

/* Take a snapshot of the rules.  Returns NULL on error. */
struct iptc_handle *iptc_init(const char *tablename);

/* Cleanup after iptc_init(). */
void iptc_free(struct iptc_handle *h);

/* Iterator functions to run through the chains.  Returns NULL at end. */
const char *iptc_first_chain(struct iptc_handle *handle);
const char *iptc_next_chain(struct iptc_handle *handle);

/* Get first rule in the given chain: NULL for empty chain. */
const struct ipt_entry *iptc_first_rule(const char *chain,
					struct iptc_handle *handle);

/* Returns NULL when rules run out. */
const struct ipt_entry *iptc_next_rule(const struct ipt_entry *prev,
				       struct iptc_handle *handle);

/* Returns a pointer to the target name of this entry. */
const char *iptc_get_target(const struct ipt_entry *e,
			    struct iptc_handle *handle);

/* Is this a built-in chain? */
int iptc_builtin(const char *chain, struct iptc_handle *const handle);

/* Get the policy of a given built-in chain */
const char *iptc_get_policy(const char *chain,
			    struct ipt_counters *counter,
			    struct iptc_handle *handle);

/* These functions return TRUE for OK or 0 and set errno.  If errno ==
   0, it means there was a version error (ie. upgrade libiptc). */
/* Rule numbers start at 1 for the first rule. */

/* Insert the entry `e' in chain `chain' into position `rulenum'. */
int iptc_insert_entry(const ipt_chainlabel chain,
		      const struct ipt_entry *e,
		      unsigned int rulenum,
		      struct iptc_handle *handle);

/* Atomically replace rule `rulenum' in `chain' with `e'. */
int iptc_replace_entry(const ipt_chainlabel chain,
		       const struct ipt_entry *e,
		       unsigned int rulenum,
		       struct iptc_handle *handle);

/* Append entry `e' to chain `chain'.  Equivalent to insert with
   rulenum = length of chain. */
int iptc_append_entry(const ipt_chainlabel chain,
		      const struct ipt_entry *e,
		      struct iptc_handle *handle);

/* Check whether a mathching rule exists */
int iptc_check_entry(const ipt_chainlabel chain,
		      const struct ipt_entry *origfw,
		      unsigned char *matchmask,
		      struct iptc_handle *handle);

/* Delete the first rule in `chain' which matches `e', subject to
   matchmask (array of length == origfw) */
int iptc_delete_entry(const ipt_chainlabel chain,
		      const struct ipt_entry *origfw,
		      unsigned char *matchmask,
		      struct iptc_handle *handle);

/* Delete the rule in position `rulenum' in `chain'. */
int iptc_delete_num_entry(const ipt_chainlabel chain,
			  unsigned int rulenum,
			  struct iptc_handle *handle);

/* Check the packet `e' on chain `chain'.  Returns the verdict, or
   NULL and sets errno. */
const char *iptc_check_packet(const ipt_chainlabel chain,
			      struct ipt_entry *entry,
			      struct iptc_handle *handle);

/* Flushes the entries in the given chain (ie. empties chain). */
int iptc_flush_entries(const ipt_chainlabel chain,
		       struct iptc_handle *handle);

/* Zeroes the counters in a chain. */
int iptc_zero_entries(const ipt_chainlabel chain,
		      struct iptc_handle *handle);

/* Creates a new chain. */
int iptc_create_chain(const ipt_chainlabel chain,
		      struct iptc_handle *handle);

/* Deletes a chain. */
int iptc_delete_chain(const ipt_chainlabel chain,
		      struct iptc_handle *handle);

/* Renames a chain. */
int iptc_rename_chain(const ipt_chainlabel oldname,
		      const ipt_chainlabel newname,
		      struct iptc_handle *handle);

/* Sets the policy on a built-in chain. */
int iptc_set_policy(const ipt_chainlabel chain,
		    const ipt_chainlabel policy,
		    struct ipt_counters *counters,
		    struct iptc_handle *handle);

/* Get the number of references to this chain */
int iptc_get_references(unsigned int *ref,
			const ipt_chainlabel chain,
			struct iptc_handle *handle);

/* read packet and byte counters for a specific rule */
struct ipt_counters *iptc_read_counter(const ipt_chainlabel chain,
				       unsigned int rulenum,
				       struct iptc_handle *handle);

/* zero packet and byte counters for a specific rule */
int iptc_zero_counter(const ipt_chainlabel chain,
		      unsigned int rulenum,
		      struct iptc_handle *handle);

/* set packet and byte counters for a specific rule */
int iptc_set_counter(const ipt_chainlabel chain,
		     unsigned int rulenum,
		     struct ipt_counters *counters,
		     struct iptc_handle *handle);

/* Makes the actual changes. */
int iptc_commit(struct iptc_handle *handle);

/* Get raw socket. */
int iptc_get_raw_socket(void);

/* Translates errno numbers into more human-readable form than strerror. */
const char *iptc_strerror(int err);

extern void dump_entries(struct iptc_handle *const);

#ifdef __cplusplus
}
#endif


#endif /* _LIBIPTC_H */
