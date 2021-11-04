/* parsing markdown files */
#include <stdlib.h>
#include <stdio.h>
#include <mkdio.h>

/*
 * TITLE_LEN: maximum length (bytes) for a recipe title.
 */
#define TITLE_LEN 64
/*
 * TAG_COUNT: maximum number of tags on one recipe.
 * TAG_NAME_LEN: maximum number of characters in one recipe.
 */
#define TAG_COUNT 10
#define TAG_NAME_LEN 30
/* => 10 * 30 = 300 bytes for tags */
/*
 * HTML_LEN: maximum number of bytes in output html.
 */
#define HTML_LEN (1 << 13)  /* ~8.2 KiB */

struct md {
	char title[TITLE_LEN];
	char tags[TAG_COUNT][TAG_NAME_LEN];
	char html[HTML_LEN];  /* main body/content */
};

/*
 * variable holding parsed `md` instance.
 * not thread safe. prevents you from holding on
 * to the parsed result, unless struct is copied.
 */
extern struct md _parsed_md;

static const int mkd_flags = 0
	| MKD_NOPANTS       | MKD_STRICT   | MKD_NOTABLES
	| MKD_NOSUPERSCRIPT | MKD_AUTOLINK | MKD_FENCEDCODE
	;

struct md *mdparse(FILE *);
