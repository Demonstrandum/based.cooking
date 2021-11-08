/* parsing markdown files */
#ifndef _MD_H
#define _MD_H

#include <stdlib.h>
#include <stdio.h>

#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif
#include <time.h>

#include <mkdio.h>
#include "config.h"

struct md {
	char title[TITLE_LEN];
	char tags[TAG_COUNT][TAG_NAME_LEN];
	char slug[SLUG_LEN];
	char *html;          /* article content, must be freed! */
#if GIT_INTEGRATION
	time_t mtime;        /* source file last modifed time */
	char author[32];     /*    first commit git user.name */
	char adate[32];      /*    --diff-filter=A (rfc-2822) */
	char mdate[32];      /*    --diff-filter=M (rfc-2822) */
#endif
};

/*
 * variable holding parsed `md` instance.
 * not thread safe. prevents you from holding on
 * to the parsed result, unless struct is copied.
 */
extern struct md _parsed_md;
extern MMIOT *_mmio;

/* add `MKD_NOPANTS` to disable 'smartypants'?
 * (e.g. 1/4 -> ¼, "it's" -> "it’s", &c.).
 * i'm not sure about other non-standard markdown feautures.
 */
static const int mkd_flags = 0
	| MKD_STRICT   | MKD_NOTABLES | MKD_NOSUPERSCRIPT
	| MKD_AUTOLINK | MKD_FENCEDCODE
	;

struct md *mdparse(char *, char *);

void string_from_tags(char *, char (*)[TAG_NAME_LEN]);
void tags_from_string(char (*)[TAG_NAME_LEN], char *);

#endif
