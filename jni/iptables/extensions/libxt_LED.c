/*
 * libxt_LED.c - shared library add-on to iptables to add customized LED
 *               trigger support.
 *
 * (C) 2008 Adam Nielsen <a.nielsen@shikadi.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xtables.h>
#include <linux/netfilter/xt_LED.h>

enum {
	O_LED_TRIGGER_ID = 0,
	O_LED_DELAY,
	O_LED_ALWAYS_BLINK,
};

#define s struct xt_led_info
static const struct xt_option_entry LED_opts[] = {
	{.name = "led-trigger-id", .id = O_LED_TRIGGER_ID,
	 .flags = XTOPT_MAND, .type = XTTYPE_STRING, .min = 0,
	 .max = sizeof(((struct xt_led_info *)NULL)->id) -
	        sizeof("netfilter-")},
	{.name = "led-delay", .id = O_LED_DELAY, .type = XTTYPE_STRING},
	{.name = "led-always-blink", .id = O_LED_ALWAYS_BLINK,
	 .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};
#undef s

static void LED_help(void)
{
	printf(
"LED target options:\n"
"--led-trigger-id name           suffix for led trigger name\n"
"--led-delay ms                  leave the LED on for this number of\n"
"                                milliseconds after triggering.\n"
"--led-always-blink              blink on arriving packets, even if\n"
"                                the LED is already on.\n"
	);
}

static void LED_parse(struct xt_option_call *cb)
{
	struct xt_led_info *led = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_LED_TRIGGER_ID:
		strcpy(led->id, "netfilter-");
		strcat(led->id, cb->arg);
		break;
	case O_LED_DELAY:
		if (strncasecmp(cb->arg, "inf", 3) == 0)
			led->delay = -1;
		else
			led->delay = strtoul(cb->arg, NULL, 0);
		break;
	case O_LED_ALWAYS_BLINK:
		led->always_blink = 1;
		break;
	}
}

static void LED_print(const void *ip, const struct xt_entry_target *target,
		      int numeric)
{
	const struct xt_led_info *led = (void *)target->data;
	const char *id = led->id + strlen("netfilter-"); /* trim off prefix */

	printf(" led-trigger-id:\"");
	/* Escape double quotes and backslashes in the ID */
	while (*id != '\0') {
		if (*id == '"' || *id == '\\')
			printf("\\");
		printf("%c", *id++);
	}
	printf("\"");

	if (led->delay == -1)
		printf(" led-delay:inf");
	else
		printf(" led-delay:%dms", led->delay);

	if (led->always_blink)
		printf(" led-always-blink");
}

static void LED_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_led_info *led = (void *)target->data;
	const char *id = led->id + strlen("netfilter-"); /* trim off prefix */

	printf(" --led-trigger-id \"");
	/* Escape double quotes and backslashes in the ID */
	while (*id != '\0') {
		if (*id == '"' || *id == '\\')
			printf("\\");
		printf("%c", *id++);
	}
	printf("\"");

	/* Only print the delay if it's not zero (the default) */
	if (led->delay > 0)
		printf(" --led-delay %d", led->delay);
	else if (led->delay == -1)
		printf(" --led-delay inf");

	/* Only print always_blink if it's not set to the default */
	if (led->always_blink)
		printf(" --led-always-blink");
}

static struct xtables_target led_tg_reg = {
	.version       = XTABLES_VERSION,
	.name          = "LED",
	.family        = PF_UNSPEC,
	.revision      = 0,
	.size          = XT_ALIGN(sizeof(struct xt_led_info)),
	.userspacesize = offsetof(struct xt_led_info, internal_data),
	.help          = LED_help,
	.print         = LED_print,
	.save          = LED_save,
	.x6_parse      = LED_parse,
	.x6_options    = LED_opts,
};

void _init(void)
{
	xtables_register_target(&led_tg_reg);
}
