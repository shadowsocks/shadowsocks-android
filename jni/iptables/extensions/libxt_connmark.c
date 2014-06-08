/* Shared library add-on to iptables to add connmark matching support.
 *
 * (C) 2002,2004 MARA Systems AB <http://www.marasystems.com>
 * by Henrik Nordstrom <hno@marasystems.com>
 *
 * Version 1.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_connmark.h>

struct xt_connmark_info {
	unsigned long mark, mask;
	uint8_t invert;
};

enum {
	O_MARK = 0,
};

static void connmark_mt_help(void)
{
	printf(
"connmark match options:\n"
"[!] --mark value[/mask]    Match ctmark value with optional mask\n");
}

static const struct xt_option_entry connmark_mt_opts[] = {
	{.name = "mark", .id = O_MARK, .type = XTTYPE_MARKMASK32,
	 .flags = XTOPT_MAND | XTOPT_INVERT},
	XTOPT_TABLEEND,
};

static void connmark_mt_parse(struct xt_option_call *cb)
{
	struct xt_connmark_mtinfo1 *info = cb->data;

	xtables_option_parse(cb);
	if (cb->invert)
		info->invert = true;
	info->mark = cb->val.mark;
	info->mask = cb->val.mask;
}

static void connmark_parse(struct xt_option_call *cb)
{
	struct xt_connmark_info *markinfo = cb->data;

	xtables_option_parse(cb);
	markinfo->mark = cb->val.mark;
	markinfo->mask = cb->val.mask;
	if (cb->invert)
		markinfo->invert = 1;
}

static void print_mark(unsigned int mark, unsigned int mask)
{
	if (mask != 0xffffffffU)
		printf(" 0x%x/0x%x", mark, mask);
	else
		printf(" 0x%x", mark);
}

static void
connmark_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_connmark_info *info = (const void *)match->data;

	printf(" CONNMARK match ");
	if (info->invert)
		printf("!");
	print_mark(info->mark, info->mask);
}

static void
connmark_mt_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_connmark_mtinfo1 *info = (const void *)match->data;

	printf(" connmark match ");
	if (info->invert)
		printf("!");
	print_mark(info->mark, info->mask);
}

static void connmark_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_connmark_info *info = (const void *)match->data;

	if (info->invert)
		printf(" !");

	printf(" --mark");
	print_mark(info->mark, info->mask);
}

static void
connmark_mt_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_connmark_mtinfo1 *info = (const void *)match->data;

	if (info->invert)
		printf(" !");

	printf(" --mark");
	print_mark(info->mark, info->mask);
}

static struct xtables_match connmark_mt_reg[] = {
	{
		.family        = NFPROTO_UNSPEC,
		.name          = "connmark",
		.revision      = 0,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_connmark_info)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_connmark_info)),
		.help          = connmark_mt_help,
		.print         = connmark_print,
		.save          = connmark_save,
		.x6_parse      = connmark_parse,
		.x6_options    = connmark_mt_opts,
	},
	{
		.version       = XTABLES_VERSION,
		.name          = "connmark",
		.revision      = 1,
		.family        = NFPROTO_UNSPEC,
		.size          = XT_ALIGN(sizeof(struct xt_connmark_mtinfo1)),
		.userspacesize = XT_ALIGN(sizeof(struct xt_connmark_mtinfo1)),
		.help          = connmark_mt_help,
		.print         = connmark_mt_print,
		.save          = connmark_mt_save,
		.x6_parse      = connmark_mt_parse,
		.x6_options    = connmark_mt_opts,
	},
};

void _init(void)
{
	xtables_register_matches(connmark_mt_reg, ARRAY_SIZE(connmark_mt_reg));
}
