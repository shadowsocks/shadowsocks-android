#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <linux/ip.h>

#ifndef IPTOS_NORMALSVC
#	define IPTOS_NORMALSVC 0
#endif

struct tos_value_mask {
	uint8_t value, mask;
};

static const struct tos_symbol_info {
	unsigned char value;
	const char *name;
} tos_symbol_names[] = {
	{IPTOS_LOWDELAY,    "Minimize-Delay"},
	{IPTOS_THROUGHPUT,  "Maximize-Throughput"},
	{IPTOS_RELIABILITY, "Maximize-Reliability"},
	{IPTOS_MINCOST,     "Minimize-Cost"},
	{IPTOS_NORMALSVC,   "Normal-Service"},
	{},
};

static bool tos_try_print_symbolic(const char *prefix,
    uint8_t value, uint8_t mask)
{
	const struct tos_symbol_info *symbol;

	if (mask != 0x3F)
		return false;

	for (symbol = tos_symbol_names; symbol->name != NULL; ++symbol)
		if (value == symbol->value) {
			printf(" %s%s", prefix, symbol->name);
			return true;
		}

	return false;
}
