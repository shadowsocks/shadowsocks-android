#ifndef PARSER_H_THU_JAN_11_04_49_38_2007
#define PARSER_H_THU_JAN_11_04_49_38_2007

#include <stdio.h>
#include <stdbool.h>

typedef enum {
	pt_bool,      // "bool" from stdbool.h, not "_Bool" or anything else
	pt_pchar,
	pt_uint16,
	pt_in_addr,
	pt_in_addr2,  // inaddr[0] = net, inaddr[1] = netmask
} parser_type;

typedef struct parser_entry_t {
	const char    *key;
	parser_type    type;
	void          *addr;
} parser_entry;


typedef struct parser_context_t parser_context;


typedef struct parser_section_t parser_section;
typedef int  (*parser_section_onenter)(parser_section *section);
typedef int  (*parser_section_onexit)(parser_section *section);

struct parser_section_t {
	parser_section         *next;
	parser_context         *context;
	const char             *name;
	parser_section_onenter  onenter; // is called on entry to section
	parser_section_onexit   onexit;  // is called on exit from section
	parser_entry           *entries;
	void                   *data;
};



typedef void (*parser_errhandler)(const char *errmsg, int line);

parser_context* parser_start(FILE *fd, parser_errhandler errhandler);
void parser_add_section(parser_context *context, parser_section *section);
int parser_run(parser_context *context);
void parser_error(parser_context *context, const char *msg);
void parser_stop(parser_context *context);

/* vim:set tabstop=4 softtabstop=4 shiftwidth=4: */
/* vim:set foldmethod=marker foldlevel=32 foldmarker={,}: */
#endif /* PARSER_H_THU_JAN_11_04_49_38_2007 */
