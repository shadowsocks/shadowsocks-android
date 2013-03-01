/*
 *	libxt_time - iptables part for xt_time
 *	Copyright © CC Computer Consultants GmbH, 2007
 *	Contact: <jengelh@computergmbh.de>
 *
 *	libxt_time.c is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 or 3 of the License.
 *
 *	Based on libipt_time.c.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <linux/types.h>
#include <linux/netfilter/xt_time.h>
#include <xtables.h>

enum {
	O_DATE_START = 0,
	O_DATE_STOP,
	O_TIME_START,
	O_TIME_STOP,
	O_MONTHDAYS,
	O_WEEKDAYS,
	O_LOCAL_TZ,
	O_UTC,
	O_KERNEL_TZ,
	F_LOCAL_TZ  = 1 << O_LOCAL_TZ,
	F_UTC       = 1 << O_UTC,
	F_KERNEL_TZ = 1 << O_KERNEL_TZ,
};

static const char *const week_days[] = {
	NULL, "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun",
};

static const struct xt_option_entry time_opts[] = {
	{.name = "datestart", .id = O_DATE_START, .type = XTTYPE_STRING},
	{.name = "datestop", .id = O_DATE_STOP, .type = XTTYPE_STRING},
	{.name = "timestart", .id = O_TIME_START, .type = XTTYPE_STRING},
	{.name = "timestop", .id = O_TIME_STOP, .type = XTTYPE_STRING},
	{.name = "weekdays", .id = O_WEEKDAYS, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "monthdays", .id = O_MONTHDAYS, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "localtz", .id = O_LOCAL_TZ, .type = XTTYPE_NONE,
	 .excl = F_UTC},
	{.name = "utc", .id = O_UTC, .type = XTTYPE_NONE,
	 .excl = F_LOCAL_TZ | F_KERNEL_TZ},
	{.name = "kerneltz", .id = O_KERNEL_TZ, .type = XTTYPE_NONE,
	 .excl = F_UTC},
	XTOPT_TABLEEND,
};

static void time_help(void)
{
	printf(
"time match options:\n"
"    --datestart time     Start and stop time, to be given in ISO 8601\n"
"    --datestop time      (YYYY[-MM[-DD[Thh[:mm[:ss]]]]])\n"
"    --timestart time     Start and stop daytime (hh:mm[:ss])\n"
"    --timestop time      (between 00:00:00 and 23:59:59)\n"
"[!] --monthdays value    List of days on which to match, separated by comma\n"
"                         (Possible days: 1 to 31; defaults to all)\n"
"[!] --weekdays value     List of weekdays on which to match, sep. by comma\n"
"                         (Possible days: Mon,Tue,Wed,Thu,Fri,Sat,Sun or 1 to 7\n"
"                         Defaults to all weekdays.)\n"
"    --kerneltz           Work with the kernel timezone instead of UTC\n");
}

static void time_init(struct xt_entry_match *m)
{
	struct xt_time_info *info = (void *)m->data;

	/* By default, we match on every day, every daytime */
	info->monthdays_match = XT_TIME_ALL_MONTHDAYS;
	info->weekdays_match  = XT_TIME_ALL_WEEKDAYS;
	info->daytime_start   = XT_TIME_MIN_DAYTIME;
	info->daytime_stop    = XT_TIME_MAX_DAYTIME;

	/* ...and have no date-begin or date-end boundary */
	info->date_start = 0;
	info->date_stop  = INT_MAX;
}

static time_t time_parse_date(const char *s, bool end)
{
	unsigned int month = 1, day = 1, hour = 0, minute = 0, second = 0;
	unsigned int year  = end ? 2038 : 1970;
	const char *os = s;
	struct tm tm;
	time_t ret;
	char *e;

	year = strtoul(s, &e, 10);
	if ((*e != '-' && *e != '\0') || year < 1970 || year > 2038)
		goto out;
	if (*e == '\0')
		goto eval;

	s = e + 1;
	month = strtoul(s, &e, 10);
	if ((*e != '-' && *e != '\0') || month > 12)
		goto out;
	if (*e == '\0')
		goto eval;

	s = e + 1;
	day = strtoul(s, &e, 10);
	if ((*e != 'T' && *e != '\0') || day > 31)
		goto out;
	if (*e == '\0')
		goto eval;

	s = e + 1;
	hour = strtoul(s, &e, 10);
	if ((*e != ':' && *e != '\0') || hour > 23)
		goto out;
	if (*e == '\0')
		goto eval;

	s = e + 1;
	minute = strtoul(s, &e, 10);
	if ((*e != ':' && *e != '\0') || minute > 59)
		goto out;
	if (*e == '\0')
		goto eval;

	s = e + 1;
	second = strtoul(s, &e, 10);
	if (*e != '\0' || second > 59)
		goto out;

 eval:
	tm.tm_year = year - 1900;
	tm.tm_mon  = month - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min  = minute;
	tm.tm_sec  = second;
	tm.tm_isdst = 0;
	/*
	 * Offsetting, if any, is done by xt_time.ko,
	 * so we have to disable it here in userspace.
	 */
	setenv("TZ", "UTC", true);
	tzset();
	ret = mktime(&tm);
	if (ret >= 0)
		return ret;
	perror("mktime");
	xtables_error(OTHER_PROBLEM, "mktime returned an error");

 out:
	xtables_error(PARAMETER_PROBLEM, "Invalid date \"%s\" specified. Should "
	           "be YYYY[-MM[-DD[Thh[:mm[:ss]]]]]", os);
	return -1;
}

static unsigned int time_parse_minutes(const char *s)
{
	unsigned int hour, minute, second = 0;
	char *e;

	hour = strtoul(s, &e, 10);
	if (*e != ':' || hour > 23)
		goto out;

	s = e + 1;
	minute = strtoul(s, &e, 10);
	if ((*e != ':' && *e != '\0') || minute > 59)
		goto out;
	if (*e == '\0')
		goto eval;

	s = e + 1;
	second = strtoul(s, &e, 10);
	if (*e != '\0' || second > 59)
		goto out;

 eval:
	return 60 * 60 * hour + 60 * minute + second;

 out:
	xtables_error(PARAMETER_PROBLEM, "invalid time \"%s\" specified, "
	           "should be hh:mm[:ss] format and within the boundaries", s);
	return -1;
}

static const char *my_strseg(char *buf, unsigned int buflen,
    const char **arg, char delim)
{
	const char *sep;

	if (*arg == NULL || **arg == '\0')
		return NULL;
	sep = strchr(*arg, delim);
	if (sep == NULL) {
		snprintf(buf, buflen, "%s", *arg);
		*arg = NULL;
		return buf;
	}
	snprintf(buf, buflen, "%.*s", (unsigned int)(sep - *arg), *arg);
	*arg = sep + 1;
	return buf;
}

static uint32_t time_parse_monthdays(const char *arg)
{
	char day[3], *err = NULL;
	uint32_t ret = 0;
	unsigned int i;

	while (my_strseg(day, sizeof(day), &arg, ',') != NULL) {
		i = strtoul(day, &err, 0);
		if ((*err != ',' && *err != '\0') || i > 31)
			xtables_error(PARAMETER_PROBLEM,
			           "%s is not a valid day for --monthdays", day);
		ret |= 1 << i;
	}

	return ret;
}

static unsigned int time_parse_weekdays(const char *arg)
{
	char day[4], *err = NULL;
	unsigned int i, ret = 0;
	bool valid;

	while (my_strseg(day, sizeof(day), &arg, ',') != NULL) {
		i = strtoul(day, &err, 0);
		if (*err == '\0') {
			if (i == 0)
				xtables_error(PARAMETER_PROBLEM,
				           "No, the week does NOT begin with Sunday.");
			ret |= 1 << i;
			continue;
		}

		valid = false;
		for (i = 1; i < ARRAY_SIZE(week_days); ++i)
			if (strncmp(day, week_days[i], 2) == 0) {
				ret |= 1 << i;
				valid = true;
			}

		if (!valid)
			xtables_error(PARAMETER_PROBLEM,
			           "%s is not a valid day specifier", day);
	}

	return ret;
}

static void time_parse(struct xt_option_call *cb)
{
	struct xt_time_info *info = cb->data;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_DATE_START:
		info->date_start = time_parse_date(cb->arg, false);
		break;
	case O_DATE_STOP:
		info->date_stop = time_parse_date(cb->arg, true);
		break;
	case O_TIME_START:
		info->daytime_start = time_parse_minutes(cb->arg);
		break;
	case O_TIME_STOP:
		info->daytime_stop = time_parse_minutes(cb->arg);
		break;
	case O_LOCAL_TZ:
		fprintf(stderr, "WARNING: --localtz is being replaced by "
		        "--kerneltz, since \"local\" is ambiguous. Note the "
		        "kernel timezone has caveats - "
		        "see manpage for details.\n");
		/* fallthrough */
	case O_KERNEL_TZ:
		info->flags |= XT_TIME_LOCAL_TZ;
		break;
	case O_MONTHDAYS:
		info->monthdays_match = time_parse_monthdays(cb->arg);
		if (cb->invert)
			info->monthdays_match ^= XT_TIME_ALL_MONTHDAYS;
		break;
	case O_WEEKDAYS:
		info->weekdays_match = time_parse_weekdays(cb->arg);
		if (cb->invert)
			info->weekdays_match ^= XT_TIME_ALL_WEEKDAYS;
		break;
	}
}

static void time_print_date(time_t date, const char *command)
{
	struct tm *t;

	/* If it is the default value, do not print it. */
	if (date == 0 || date == LONG_MAX)
		return;

	t = gmtime(&date);
	if (command != NULL)
		/*
		 * Need a contiguous string (no whitespaces), hence using
		 * the ISO 8601 "T" variant.
		 */
		printf(" %s %04u-%02u-%02uT%02u:%02u:%02u",
		       command, t->tm_year + 1900, t->tm_mon + 1,
		       t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	else
		printf(" %04u-%02u-%02u %02u:%02u:%02u",
		       t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
		       t->tm_hour, t->tm_min, t->tm_sec);
}

static void time_print_monthdays(uint32_t mask, bool human_readable)
{
	unsigned int i, nbdays = 0;

	printf(" ");
	for (i = 1; i <= 31; ++i)
		if (mask & (1 << i)) {
			if (nbdays++ > 0)
				printf(",");
			printf("%u", i);
			if (human_readable)
				switch (i % 10) {
					case 1:
						printf("st");
						break;
					case 2:
						printf("nd");
						break;
					case 3:
						printf("rd");
						break;
					default:
						printf("th");
						break;
				}
		}
}

static void time_print_weekdays(unsigned int mask)
{
	unsigned int i, nbdays = 0;

	printf(" ");
	for (i = 1; i <= 7; ++i)
		if (mask & (1 << i)) {
			if (nbdays > 0)
				printf(",%s", week_days[i]);
			else
				printf("%s", week_days[i]);
			++nbdays;
		}
}

static inline void divide_time(unsigned int fulltime, unsigned int *hours,
    unsigned int *minutes, unsigned int *seconds)
{
	*seconds  = fulltime % 60;
	fulltime /= 60;
	*minutes  = fulltime % 60;
	*hours    = fulltime / 60;
}

static void time_print(const void *ip, const struct xt_entry_match *match,
                       int numeric)
{
	const struct xt_time_info *info = (const void *)match->data;
	unsigned int h, m, s;

	printf(" TIME");

	if (info->daytime_start != XT_TIME_MIN_DAYTIME ||
	    info->daytime_stop != XT_TIME_MAX_DAYTIME) {
		divide_time(info->daytime_start, &h, &m, &s);
		printf(" from %02u:%02u:%02u", h, m, s);
		divide_time(info->daytime_stop, &h, &m, &s);
		printf(" to %02u:%02u:%02u", h, m, s);
	}
	if (info->weekdays_match != XT_TIME_ALL_WEEKDAYS) {
		printf(" on");
		time_print_weekdays(info->weekdays_match);
	}
	if (info->monthdays_match != XT_TIME_ALL_MONTHDAYS) {
		printf(" on");
		time_print_monthdays(info->monthdays_match, true);
	}
	if (info->date_start != 0) {
		printf(" starting from");
		time_print_date(info->date_start, NULL);
	}
	if (info->date_stop != INT_MAX) {
		printf(" until date");
		time_print_date(info->date_stop, NULL);
	}
	if (!(info->flags & XT_TIME_LOCAL_TZ))
		printf(" UTC");
}

static void time_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_time_info *info = (const void *)match->data;
	unsigned int h, m, s;

	if (info->daytime_start != XT_TIME_MIN_DAYTIME ||
	    info->daytime_stop != XT_TIME_MAX_DAYTIME) {
		divide_time(info->daytime_start, &h, &m, &s);
		printf(" --timestart %02u:%02u:%02u", h, m, s);
		divide_time(info->daytime_stop, &h, &m, &s);
		printf(" --timestop %02u:%02u:%02u", h, m, s);
	}
	if (info->monthdays_match != XT_TIME_ALL_MONTHDAYS) {
		printf(" --monthdays");
		time_print_monthdays(info->monthdays_match, false);
	}
	if (info->weekdays_match != XT_TIME_ALL_WEEKDAYS) {
		printf(" --weekdays");
		time_print_weekdays(info->weekdays_match);
	}
	time_print_date(info->date_start, "--datestart");
	time_print_date(info->date_stop, "--datestop");
	if (info->flags & XT_TIME_LOCAL_TZ)
		printf(" --kerneltz");
}

static struct xtables_match time_match = {
	.name          = "time",
	.family        = NFPROTO_UNSPEC,
	.version       = XTABLES_VERSION,
	.size          = XT_ALIGN(sizeof(struct xt_time_info)),
	.userspacesize = XT_ALIGN(sizeof(struct xt_time_info)),
	.help          = time_help,
	.init          = time_init,
	.print         = time_print,
	.save          = time_save,
	.x6_parse      = time_parse,
	.x6_options    = time_opts,
};

void _init(void)
{
	xtables_register_match(&time_match);
}
