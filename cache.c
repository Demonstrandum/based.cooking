#include "cache.h"
#include "config.h"
#include "based.h"
#include "md.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

/* Build cache file format:
 *	<slug>:\n
 *	\t<modified-epoch>\n
 *	\t<title>\n
 *	\t<tags>\n
 *	\t<author-name>\n
 *	\t<ymd-posted> <ymd-edited>\n
 *	\t<rfc2822-published-date>\n
 * Format is repeated though the file for each recipe.
 */

static const char SCAN_CACHE_ENTRY[] = {
	"%[^:]%*[^\n]%*c" /*    1 |            slug */
	"\t%lu%*c"        /*    2 |           epoch */
	"\t%[^\n]%*c"     /*    3 |           title */
	"\t%[^\n]%*c"     /*    4 |            tags */
	"\t%[^\n]%*c"     /*    5 |          author */
	"\t%s %[^\n]%*c"  /* 6, 7 | A-date + M-date */
	"\t%[^\n]%*c"     /*    8 |  rfc2822 A-date */
};


static const char FMT_CACHE_ENTRY[] = {
	"%s:\n"      /*            slug */
	"\t%lu\n"    /*           epoch */
	"\t%s\n"     /*           title */
	"\t%s\n"     /*            tags */
	"\t%s\n"     /*          author */
	"\t%s %s\n"  /* A-date + M-date */
	"\t%s\n"     /*  rfc2822 A-date */
};

/* build cache populates `struct md` recipe entries with everything
 * except the html.
 * unfortunately the html must still be populated in order to rebuild
 * the RSS and Atom feeds, so source files must still be read and parsed.
 */

/* recipe slugs should be added alphabetically with scandir.
 * this is necessary for concurrent traversal of the directory
 * listing along with the cache entries.
 * this avoids having to implement a binary search or a hash-
 * table to find cache entries.
 */

void
init_cache(struct cache *c, char *filename)
{
	if (0 != access(filename, F_OK)) {
		c->file = fopen(filename, "w");
		if (NULL == c->file) die("failed to create cache.");
		fclose(c->file); c->file = NULL;
	}
	c->file = fopen(filename, "r+");
	if (NULL == c->file) die("failed to open cache.");

	strcpy(c->filename, filename);

	c->entries = calloc(MAX_RECIPES, sizeof(struct md));
	if (NULL == c->entries) die("failed to allocate cache.");
	c->size = 0;
	c->count = 0;
}

void
parse_cache(struct cache *c)
{
	char tags[TAG_COUNT * TAG_NAME_LEN] = { 0 };  /*< space separated */
	long unsigned epoch;
	int assigns;
	struct md *entry;
	/* cache file is first read in entirety, and walked through along with
	 * the file listing in alphabetical (slug) order. as soon as a file is
	 * missing form the cache (a new file) we insert_cache() it into that
	 * location with the newly populated recipe (struct md), and offset all
	 * other caches. walking may continue as previous.
	 */

	c->size = 0;
	while (1) {
		entry = &c->entries[c->size];
		assigns = fscanf(c->file, SCAN_CACHE_ENTRY, entry->slug,
			&epoch,
			entry->title,
			tags,
			entry->author,
			entry->adate, entry->mdate,
			entry->published);
		/* a cache entry has 8 assignments */
		if (8 != assigns) {
			if (EOF == assigns) {
				if (ferror(c->file)) die("failed to read cache.");
			} else {
				fprintf(stderr,
					"corrupted cache file (%s). "
					"missing %d variables in entry number %lu.\n",
					c->filename, 8 - assigns, c->count + 1);
				exit(EXIT_FAILURE);
			}
			break;  /* file finished */
		}
		else { ++c->size; }

		/* write in epoch and tags */
		entry->mtime = epoch;  /* performs implicit cast */
		tags_from_string(entry->tags, tags);
	}
}

struct md *
next_cache(struct cache *c)
{
	++c->count;
	if (c->count - 1 >= c->size)
		return NULL;
	return &c->entries[c->count - 1];
}

struct md *
insert_cache(struct cache *c, struct md *entry)
{
	/* insert above entry */
	size_t tail;
	struct md *blank;

	blank = c->entries + c->count - 1;
	tail = c->size - c->count + 1;

	/* if new entry comes after cached entry in the alphabet,
	 * the cached entry must be a deleted file, and should
	 * be itself deleted from the cache. */
	if (blank->title[0] != '\0' && 0 < strcoll(entry->slug, blank->slug)) {
		memcpy(blank, entry, sizeof(struct md));
		return blank;  /* entry overwritten */
	}

	/* offset tail of array leaving a blank entry above */
	memmove(blank + 1, blank, tail * sizeof(struct md));
	/* fill in blank */
	memcpy(blank, entry, sizeof(struct md));

	++c->size;

	return blank;
}

struct md *
update_cache(struct cache *c, struct md *entry)
{
	struct md *current = c->entries + c->count - 1;
	if (0 != strcmp(entry->slug, current->slug)) {
		fprintf(stderr, "Tried to update a cache with non-matching filename.\n");
		fprintf(stderr, "\t%s != %s\n", current->slug, entry->slug);
		exit(EXIT_FAILURE);
	}
	/* overwrite */
	memcpy(current, entry, sizeof(struct md));
	return current;
}

void
dump_cache(struct cache *c)
{
	char tags[TAG_COUNT * TAG_NAME_LEN] = { 0 };  /*< space separated */
	size_t i;
	struct md *entry;

	/* overwrite cache */
	ftruncate(fileno(c->file), 0);
	rewind(c->file);
	for (i = 0; i < c->size; ++i) {
		entry = &c->entries[i];

		string_from_tags(tags, entry->tags);

		fprintf(c->file, FMT_CACHE_ENTRY, entry->slug,
			entry->mtime,
			entry->title,
			tags,
			entry->author,
			entry->adate, entry->mdate,
			entry->published);
	}

	fclose(c->file);
	free(c->entries);
}


