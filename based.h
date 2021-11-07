#ifndef _BASED_H
#define _BASED_H

#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

static void
die(char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fputc('\n', stderr);
	if (errno) fprintf(stderr, " -> %s\n", strerror(errno));

	exit(errno || EXIT_FAILURE);
}

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

#endif
