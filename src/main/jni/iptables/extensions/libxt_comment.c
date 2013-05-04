/* Shared library add-on to iptables to add comment match support.
 *
 * ChangeLog
 *     2003-05-13: Brad Fisher <brad@info-link.net>
 *         Initial comment match
 *     2004-05-12: Brad Fisher <brad@info-link.net>
 *         Port to patch-o-matic-ng
 */
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_comment.h>

enum {
	O_COMMENT = 0,
};

static void comment_help(void)
{
	printf(
		"comment match options:\n"
		"--comment COMMENT             Attach a comment to a rule\n");
}

static const struct xt_option_entry comment_opts[] = {
	{.name = "comment", .id = O_COMMENT, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_PUT,
	 XTOPT_POINTER(struct xt_comment_info, comment)},
	XTOPT_TABLEEND,
};

static void
comment_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	struct xt_comment_info *commentinfo = (void *)match->data;

	commentinfo->comment[XT_MAX_COMMENT_LEN-1] = '\0';
	printf(" /* %s */", commentinfo->comment);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void
comment_save(const void *ip, const struct xt_entry_match *match)
{
	struct xt_comment_info *commentinfo = (void *)match->data;

	commentinfo->comment[XT_MAX_COMMENT_LEN-1] = '\0';
	printf(" --comment");
	xtables_save_string(commentinfo->comment);
}

static struct xtables_match comment_match = {
	.family		= NFPROTO_UNSPEC,
	.name		= "comment",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_comment_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_comment_info)),
	.help		= comment_help,
	.print 		= comment_print,
	.save 		= comment_save,
	.x6_parse	= xtables_option_parse,
	.x6_options	= comment_opts,
};

void _init(void)
{
	xtables_register_match(&comment_match);
}
