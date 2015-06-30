#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_esp.h>

enum {
	O_ESPSPI = 0,
};

static void esp_help(void)
{
	printf(
"esp match options:\n"
"[!] --espspi spi[:spi]\n"
"				match spi (range)\n");
}

static const struct xt_option_entry esp_opts[] = {
	{.name = "espspi", .id = O_ESPSPI, .type = XTTYPE_UINT32RC,
	 .flags = XTOPT_INVERT | XTOPT_PUT,
	 XTOPT_POINTER(struct xt_esp, spis)},
	XTOPT_TABLEEND,
};

static void esp_parse(struct xt_option_call *cb)
{
	struct xt_esp *espinfo = cb->data;

	xtables_option_parse(cb);
	if (cb->nvals == 1)
		espinfo->spis[1] = espinfo->spis[0];
	if (cb->invert)
		espinfo->invflags |= XT_ESP_INV_SPI;
}

static void
print_spis(const char *name, uint32_t min, uint32_t max,
	    int invert)
{
	const char *inv = invert ? "!" : "";

	if (min != 0 || max != 0xFFFFFFFF || invert) {
		if (min == max)
			printf(" %s:%s%u", name, inv, min);
		else
			printf(" %ss:%s%u:%u", name, inv, min, max);
	}
}

static void
esp_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_esp *esp = (struct xt_esp *)match->data;

	printf(" esp");
	print_spis("spi", esp->spis[0], esp->spis[1],
		    esp->invflags & XT_ESP_INV_SPI);
	if (esp->invflags & ~XT_ESP_INV_MASK)
		printf(" Unknown invflags: 0x%X",
		       esp->invflags & ~XT_ESP_INV_MASK);
}

static void esp_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_esp *espinfo = (struct xt_esp *)match->data;

	if (!(espinfo->spis[0] == 0
	    && espinfo->spis[1] == 0xFFFFFFFF)) {
		printf("%s --espspi ",
			(espinfo->invflags & XT_ESP_INV_SPI) ? " !" : "");
		if (espinfo->spis[0]
		    != espinfo->spis[1])
			printf("%u:%u",
			       espinfo->spis[0],
			       espinfo->spis[1]);
		else
			printf("%u",
			       espinfo->spis[0]);
	}

}

static struct xtables_match esp_match = {
	.family		= NFPROTO_UNSPEC,
	.name 		= "esp",
	.version 	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_esp)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_esp)),
	.help		= esp_help,
	.print		= esp_print,
	.save		= esp_save,
	.x6_parse	= esp_parse,
	.x6_options	= esp_opts,
};

void
_init(void)
{
	xtables_register_match(&esp_match);
}
