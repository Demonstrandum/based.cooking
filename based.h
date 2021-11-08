#ifndef _BASED_H
#define _BASED_H

#include "config.h"
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif
#include <time.h>

#ifndef TAG_NAME_LEN
#error "You need to #define the max TAG_NAME_LEN."
#endif

#ifndef TAG_COUNT
#error "You need to #define the max TAG_COUNT."
#endif

#ifndef TITLE_LEN
#error "You need to #define the max TITLE_LEN."
#endif

/* linked list of all tags alphabetically inserted. */
struct taglist {
	char name[TAG_NAME_LEN];
	struct taglist *next;
};

/* linked list of all titles alphabetically inserted. */
struct recipelist {
	char title[TITLE_LEN];
	char url[TITLE_LEN];
	char tags[TAG_COUNT][TAG_NAME_LEN];
	struct recipelist *next;
};

void die(char *, ...);
char *from_rfc2822(const char *, char *, size_t, const char *);
char *rfc3339time(char *, struct tm *);
char *to_rfc3339(char *, const char *, const char *);

#endif
