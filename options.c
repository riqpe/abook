
/*
 * $Id$
 *
 * by JH <jheinonen@users.sourceforge.net>
 *
 * Copyright (C) Jaakko Heinonen
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "options.h"
#include "abook.h"
#include "misc.h"

#ifndef FALSE
#	define FALSE	0
#endif
#ifndef TRUE
#	define TRUE	1
#endif

#define UL	(unsigned long)

/*
 * option types
 */

enum opt_type {
	OT_BOOL,
	OT_STR,
	OT_INT
};

struct option {
	char *option;
	enum opt_type type;
	unsigned int data;
	unsigned long init;
};

static struct option abook_vars[] = {
	{ "autosave", OT_BOOL, BOOL_AUTOSAVE, TRUE },

	{ "show_all_emails", OT_BOOL, BOOL_SHOW_ALL_EMAILS, TRUE },
	{ "emailpos", OT_INT, INT_EMAILPOS, 25 },
	{ "extra_column", OT_STR, STR_EXTRA_COLUMN, UL "phone" },
	{ "extra_alternative", OT_STR, STR_EXTRA_ALTERNATIVE, UL "-1" },
	{ "extrapos", OT_INT, INT_EXTRAPOS, 65 },

	{ "mutt_command", OT_STR, STR_MUTT_COMMAND, UL "mutt" },
	{ "mutt_return_all_emails", OT_BOOL, BOOL_MUTT_RETURN_ALL_EMAILS,
		TRUE },
	
	{ "print_command", OT_STR, STR_PRINT_COMMAND, UL "lpr" },

	{ "www_command", OT_STR, STR_WWW_COMMAND, UL "lynx" },
	
	{ "address_style", OT_STR, STR_ADDRESS_STYLE, UL "eu" },

	{ "use_ascii_only", OT_BOOL, BOOL_USE_ASCII_ONLY, FALSE },

	{ NULL }
};

static unsigned char bool_opts[BOOL_MAX];
static int int_opts[INT_MAX];
static char *str_opts[STR_MAX];

static void
set_int(enum int_opts opt, int value)
{
	assert(opt >= 0 && opt < INT_MAX);

	int_opts[opt] = value;
}

static void
set_bool(enum bool_opts opt, bool value)
{
	assert(opt >= 0 && opt < BOOL_MAX);

	bool_opts[opt] = value;
}

static void
set_str(enum str_opts opt, char *value)
{
	assert(opt >= 0 && opt < STR_MAX);

	if(str_opts[opt])
		free(str_opts[opt]);

	str_opts[opt] = strdup(value);
}

int
opt_get_int(enum int_opts opt)
{
	assert(opt >= 0 && opt < INT_MAX);

	return int_opts[opt];
}

bool
opt_get_bool(enum bool_opts opt)
{
	assert(opt >= 0 && opt < STR_MAX);

	return bool_opts[opt];
}

char *
opt_get_str(enum str_opts opt)
{
	assert(opt >= 0 && opt < STR_MAX);

	return str_opts[opt];
}

static void
restore_default(struct option *p)
{
	switch(p -> type) {
		case OT_BOOL:
			set_bool(p -> data, (bool)p -> init);
			break;
		case OT_INT:
			set_int(p -> data, (int)p -> init);
			break;
		case OT_STR:
			if(p -> init)
				set_str(p -> data, (char *) p -> init);
			break;
		default:
			assert(0);
	}
}

void
init_opts()
{
	int i;

	for(i = 0; abook_vars[i].option; i++)
		restore_default(&abook_vars[i]);
}

void
free_opts()
{
	int i;

	/*
	 * only strings need to be freed
	 */
	for(i = 0; i < STR_MAX; i++) {
		free(str_opts[i]);
		str_opts[i] = NULL;
	}
}

/*
 * file parsing
 */

static void
opt_line_remove_comments(char *p)
{
	bool in_quote = FALSE;
	bool escape = FALSE;

	assert(p != NULL);

	for(; *p; p++) {
		switch(*p) {
			case '\"':
				if(!escape) {
					in_quote = !in_quote;
					escape = FALSE;
				}
				break;
			case '\\':
				escape = TRUE;
				break;
			case '#':
				if(!in_quote) {
					*p = 0;
					return;
				}
			default:
				escape = FALSE;
		}
	}
}

static char *
get_token_start(char *p)
{
	assert(p);
	
	for(; ISSPACE(*p); p++);

	return p;
}

static char *
get_token_end(char *p)
{
	assert(p);

	for(p = get_token_start(p); *p; p++) {
		if(ISSPACE(*p)) {
			break;
		}
	}

	return p;
}

static char *
opt_set_set_option(char *var, char *p, struct option *opt)
{
	int len;
	
	strtrim(p);

	len = strlen(p);

	if(p[len - 1] == '\"' && *p == '\"') {
		if(len < 3)
			return "invalid value";
		p[len - 1] = 0;
		p++;
	}

	switch(opt -> type) {
		case OT_STR:
			set_str(opt -> data, p);
			break;
		case OT_INT:
			set_int(opt -> data, safe_atoi(p));
			break;
		case OT_BOOL:
			if(!strcasecmp(p, "true") || !strcasecmp(p, "on"))
				set_bool(opt -> data, TRUE);
			else if(!strcasecmp(p, "false") ||
					!strcasecmp(p, "off"))
				set_bool(opt -> data, FALSE);
			else
				return "invalid value";
			break;
	}
	
	return NULL;
}

static char *
opt_parse_set(char *p)
{
	char *var;
	int i;

	var = get_token_start(p);
	if((p = strchr(var, '=')))
		*p++ = 0;
	else
		return "invalid value assignment";
	
	strtrim(var);

	for(i = 0;abook_vars[i].option; i++)
		if(!strcmp(abook_vars[i].option, var))
			return opt_set_set_option(var, p, &abook_vars[i]);
	
	return "unknown option";
}


static struct {
	char *token;
	char * (*func) (char *line);
} opt_parsers[] = {
	{ "set", opt_parse_set },
	{ NULL }
};

static bool
opt_parse_line(char *line, int n, char *fn)
{
	int i;
	char *p;
	char *err = NULL;
	
	assert(line && fn);

	line = get_token_start(line);
	p = get_token_end(line);
	*p++ = 0;

	if(!*line)
		return FALSE;

	strtrim(line);
	strtrim(p);

	for(i = 0; opt_parsers[i].token; i++)
		if(!strcmp(opt_parsers[i].token, line)) {
			if(!(err = opt_parsers[i].func(p)))
				return FALSE;
			break;
		}
	
	fprintf(stderr, "%s: parse error at line %d: ", fn, n);
	if(err)
		fprintf(stderr, "%s\n", err);
	else
		fprintf(stderr, "unknown token %s\n", line);

	return TRUE;
}

int
load_opts(char *filename)
{
	FILE *in;
	char *line = NULL;
	int n;
	bool err = FALSE;
	
	if((in = fopen(filename, "r")) == NULL)
		return -1;

	
	for(n = 1;!feof(in); n++) {
		line = getaline(in);

		if(feof(in))
			break;

		if(line && *line) {
			opt_line_remove_comments(line);
			if(*line)
				err = opt_parse_line(line, n, filename);
		}

		my_free(line);
	}

	free(line);

	if(err) {
		printf("Press any key to continue...\n");
		fgetc(stdin);
	}
	
	return err;
}

