/*
 * DiffServ classname <-> DiffServ codepoint mapping functions.
 *
 * The latest list of the mappings can be found at:
 * <http://www.iana.org/assignments/dscp-registry>
 *
 * This code is released under the GNU GPL v2, 1991
 *
 * Author: Iain Barnes
 */

#include <stdio.h>
#include <string.h>
#include <xtables.h>


static const struct ds_class
{
	const char *name;
	unsigned int dscp;
} ds_classes[] =
{
	{ "CS0", 0x00 },
	{ "CS1", 0x08 },
	{ "CS2", 0x10 },
	{ "CS3", 0x18 },
	{ "CS4", 0x20 },
	{ "CS5", 0x28 },
	{ "CS6", 0x30 },
	{ "CS7", 0x38 },
	{ "BE", 0x00 },
	{ "AF11", 0x0a },
	{ "AF12", 0x0c },
	{ "AF13", 0x0e },
	{ "AF21", 0x12 },
	{ "AF22", 0x14 },
	{ "AF23", 0x16 },
	{ "AF31", 0x1a },
	{ "AF32", 0x1c },
	{ "AF33", 0x1e },
	{ "AF41", 0x22 },
	{ "AF42", 0x24 },
	{ "AF43", 0x26 },
	{ "EF", 0x2e }
};



static unsigned int
class_to_dscp(const char *name)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(ds_classes); i++) {
		if (!strncasecmp(name, ds_classes[i].name,
					strlen(ds_classes[i].name)))
			return ds_classes[i].dscp;
	}

	xtables_error(PARAMETER_PROBLEM,
			"Invalid DSCP value `%s'\n", name);
}


#if 0
static const char *
dscp_to_name(unsigned int dscp)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ds_classes); ++i) 
		if (dscp == ds_classes[i].dscp)
			return ds_classes[i].name;

	xtables_error(PARAMETER_PROBLEM,
			"Invalid DSCP value `%d'\n", dscp);
}
#endif

