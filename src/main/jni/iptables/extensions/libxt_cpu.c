#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_cpu.h>

enum {
	O_CPU = 0,
};

static void cpu_help(void)
{
	printf(
"cpu match options:\n"
"[!] --cpu number   Match CPU number\n");
}

static const struct xt_option_entry cpu_opts[] = {
	{.name = "cpu", .id = O_CPU, .type = XTTYPE_UINT32,
	 .flags = XTOPT_INVERT | XTOPT_MAND | XTOPT_PUT,
	 XTOPT_POINTER(struct xt_cpu_info, cpu)},
	XTOPT_TABLEEND,
};

static void cpu_parse(struct xt_option_call *cb)
{
	struct xt_cpu_info *cpuinfo = cb->data;

	xtables_option_parse(cb);
	if (cb->invert)
		cpuinfo->invert = true;
}

static void
cpu_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_cpu_info *info = (void *)match->data;

	printf(" cpu %s%u", info->invert ? "! ":"", info->cpu);
}

static void cpu_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_cpu_info *info = (void *)match->data;

	printf("%s --cpu %u", info->invert ? " !" : "", info->cpu);
}

static struct xtables_match cpu_match = {
	.family		= NFPROTO_UNSPEC,
 	.name		= "cpu",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_cpu_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_cpu_info)),
	.help		= cpu_help,
	.print		= cpu_print,
	.save		= cpu_save,
	.x6_parse	= cpu_parse,
	.x6_options	= cpu_opts,
};

void _init(void)
{
	xtables_register_match(&cpu_match);
}
