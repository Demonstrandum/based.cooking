/* compiles recipe files to static site. */
/* TODO:
 * - Git integration (recipe author, upload date, edit date).
 * - RSS and Atom feed generation.
 * - Caching!!!!!!!! .buildcache stores unix time stamps, rebuilds only if out of date.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <locale.h>
/* looping through directories */
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
/* parsing markdown + tags */
#include "md.h"
#include "based.h"
#include "config.h"

#define PATH_LENGTH 255
/* currently we have 249 recipes
 * increase MAX_RECIPES if we exceed that. */
#define MAX_RECIPES 400
/* currently we have 129 (!) tags.
 * increase MAX_TAGS if we exceed that. */
#define MAX_TAGS 200
#define CACHE_FILE ".buildcache"

void usage(char *prog)
{
	fprintf(stderr, "usage: %s [-hsdqC]\n", prog);
}

#define BOLD 1
#define RESET 0

static char _escbuf[256];
static int  _escofs = 0;
static char *
ansi(int code)
{
	char *esc = _escbuf + _escofs;
	_escofs += sprintf(esc, "\x1b[%dm", code) + 1;
	return esc;
}

static int verbosity = 1;
static int
logprint(char *fmt, ...)
{
	int err = 0;
	va_list args;
	va_start(args, fmt);
	if (verbosity > 0)
		err = vfprintf(stderr, fmt, args);
	va_end(args);
	return err;
}

/* contiguous array of recipes in no order */
static struct recipelist recipemem[MAX_RECIPES] = { 0 };
static size_t recipecount = 0;
/* alphabetically sorted linked list title insertion */
static void
insert_recipe(struct recipelist **node, struct md *recipe, char *slug)
{
	struct recipelist *item = &recipemem[recipecount++];
	/* place title in memory arena */
	sprintf(item->title, "%s", recipe->title);
	sprintf(item->url, "./%s.html", slug);
	memcpy(item->tags, recipe->tags, sizeof(recipe->tags));
	/* reorder title pointers */
	for (; *node != NULL; node = &(*node)->next)
		if (0 < strcoll((*node)->title, recipe->title))
			break;
	item->next = *node;
	*node = item;
}

/* contiguous array of tags in no order */
static struct taglist tagmem[MAX_TAGS] = { 0 };
static size_t tagcount = 0;
/* alphabetically sorted linked list tag insertion */
static void
insert_tags(struct taglist **tagnode, char (*tag)[TAG_NAME_LEN])
{
	struct taglist **tagroot = tagnode;
	struct taglist *tagitem = NULL;

	for (; (*tag)[0] != '\0'; ++tag) {
		/* inserting multiple tags in one call requires us
		 * to keep track of the root list node we called with */
		for (tagitem = &tagmem[0]; tagitem->name[0] != '\0'; ++tagitem)
			if (0 == strcoll(tagitem->name, *tag))
				goto next_tag;  /* tag already exists */
		/* new tag needs to be added to memory */
		tagitem = &tagmem[tagcount++];
		memcpy(tagitem->name, *tag, strlen(*tag));
		/* insert alphabetically into linked list */
		for (tagnode = tagroot; *tagnode != NULL; tagnode = &(*tagnode)->next)
			if (0 < strcoll((*tagnode)->name, tagitem->name))
				break;
		tagitem->next = *tagnode; /* point to rest */
		*tagnode = tagitem;       /* inserted */
next_tag:;
	}
}

static int
write_index(char *dst, struct taglist *tag, struct recipelist *recipe)
{
	FILE *f, *mdf;  /* index.html file, index.md file. */
	MMIOT *mmio;
	char indexfile[PATH_LENGTH];
	sprintf(indexfile, "%s/index.html", dst);

	f = fopen(indexfile, "w");
	fprintf(f, FMT_HTML_HEAD, PAGE_TITLE, DESCRIPTION, FAVICON);
	fprintf(f, "<body>\n");
	fprintf(f, FMT_HTML_BANNER, PAGE_TITLE);
	fprintf(f, FMT_HTML_INDEX_HEADER, DESCRIPTION);
	/* loop through tags */
	for (; tag != NULL; tag = tag->next) {
		fprintf(f, FMT_HTML_TAG_ENTRY, tag->name, tag->name);
		if (tag->next != NULL) fprintf(f, FMT_HTML_TAG_SEP);
	}
	fprintf(f, FMT_HTML_INDEX_LIST_START);
	/* loop through recipes */
	for (; recipe != NULL; recipe = recipe->next)
		fprintf(f, FMT_HTML_INDEX_LIST_ENTRY, recipe->url, recipe->title);
	fprintf(f, FMT_HTML_INDEX_LIST_END);
	/* parse and insert index.md file */
	mdf = fopen(INDEX_MARKDOWN, "r");
	mmio = mkd_in(mdf, mkd_flags);
	markdown(mmio, f, mkd_flags);
	fclose(mdf);
	/* end index document */
	fprintf(f, FMT_HTML_FOOTER);
	fprintf(f, "</body>\n</html>\n");
	fclose(f);

	return EXIT_SUCCESS;
}

static int
write_recipe(FILE *f, struct md *recipe)
{
	char (*tag)[TAG_NAME_LEN];
	char title[TITLE_LEN + sizeof(PAGE_TITLE) + 10] = { 0 };
	sprintf(title, "%s – %s", recipe->title, PAGE_TITLE);

	fprintf(f, FMT_HTML_HEAD, title, DESCRIPTION, FAVICON);
	fprintf(f, "<body>\n");
	fprintf(f, FMT_HTML_ARTICLE_HEADER);
	fprintf(f, "%s", recipe->html);
	fprintf(f, FMT_HTML_ARTICLE_END);
	/* loop over recipe tags */
	for (tag = &(recipe->tags[0]); (*tag)[0] != '\0'; ++tag) {
		fprintf(f, FMT_HTML_TAG_ENTRY, *tag, *tag);
		if (tag[1][0] != '\0') fprintf(f, FMT_HTML_TAG_SEP);
	}
	fprintf(f, FMT_HTML_ARTICLE_FOOTER, "", "", "");  /* TODO(git) */
	fprintf(f, FMT_HTML_FOOTER);
	fprintf(f, "</body>\n</html>\n");

	return EXIT_SUCCESS;
}

static int
write_tagfiles(char *dst, struct taglist *tags, struct recipelist *recipes)
{
	FILE *f;
	char (*rtag)[TAG_NAME_LEN];
	char tagfile[PATH_LENGTH];
	char title[TAG_NAME_LEN + sizeof(PAGE_TITLE) + 20];
	struct taglist *tag;
	struct recipelist *recipe;

	/* create a tag file for each tag, with header content */
	for (tag = tags; tag != NULL; tag = tag->next) {
		sprintf(title, "Recipes tagged %s – %s", tag->name, PAGE_TITLE);
		sprintf(tagfile, "%s/@%s.html", dst, tag->name);
		f = fopen(tagfile, "w");
		fprintf(f, FMT_HTML_HEAD, title, DESCRIPTION, FAVICON);
		fprintf(f, "<body>\n");
		fprintf(f, FMT_HTML_BANNER, PAGE_TITLE);
		fprintf(f, FMT_HTML_TAG_HEADER, tag->name);
		fprintf(f, FMT_HTML_INDEX_LIST_START);
		fclose(f);
	}

	/* go through each recipe, append the recipe
	 * to the tag files of its corresponding tags */
	for (recipe = recipes; recipe != NULL; recipe = recipe->next) {
		for (rtag = &(recipe->tags[0]); (*rtag)[0] != '\0'; ++rtag) {
			sprintf(tagfile, "%s/@%s.html", dst, *rtag);
			f = fopen(tagfile, "a");
			fprintf(f, FMT_HTML_INDEX_LIST_ENTRY, recipe->url, recipe->title);
			fclose(f);
		}
	}

	/* end the tag files with the footer content */
	for (tag = tags; tag != NULL; tag = tag->next) {
		sprintf(tagfile, "%s/@%s.html", dst, tag->name);
		f = fopen(tagfile, "a");
		fprintf(f, FMT_HTML_INDEX_LIST_END);
		fprintf(f, FMT_HTML_FOOTER);
		fprintf(f, "</body>\n</html>\n");
		fclose(f);
	}

	return EXIT_SUCCESS;
}

static int
generate(char *src, char *dst)
{
	FILE *srcf, *dstf;
	DIR *srcdir;
	struct dirent *srcent;
	/* file names */
	char *slug;
	char srcfile[PATH_LENGTH] = { '\0' };
	char dstfile[PATH_LENGTH] = { '\0' };
	/* contains html and metadata (i.e. tags) */
	struct md *recipe;
	char (*tag)[TAG_NAME_LEN];
	/* linked list of alphabetically sorted tags */
	struct taglist *tags = NULL;
	/* linked list of alphabetically sorted titles */
	struct recipelist *recipes = NULL;

	if (NULL != (srcdir = opendir(src))) {
		while (NULL != (srcent = readdir(srcdir))) {
			if (srcent->d_name[0] == '.')
				continue;  /* skip filenames starting with '.' */
			slug = srcent->d_name;
			/* trim `.md` off */
			slug[strlen(slug) - 3] = '\0';
			sprintf(srcfile, "%s/%s.md",   src, slug);
			sprintf(dstfile, "%s/%s.html", dst, slug);

			logprint("%sgenerating%s: %s -> %s\n",
				ansi(BOLD), ansi(RESET), srcfile, dstfile);

			/* open src and dst file */
			srcf = fopen(srcfile, "r");
			if (NULL == srcf) {
				fputs("file was moved.", stderr);
				return EXIT_FAILURE;
			}
			dstf = fopen(dstfile, "w");
			if (NULL == dstf) {
				fprintf(stderr, "fopen error: %s\n", strerror(errno));
				return EXIT_FAILURE;
			}
			/* convert md to html */
			recipe = mdparse(srcf);
			logprint("  ├─ title: ‘%s’\n", recipe->title);
			logprint("  ╰── tags: ");
			for (tag = &(recipe->tags[0]); (*tag)[0] != '\0'; ++tag)
				logprint("%s, ", *tag);
			logprint("\b\b \n");
			/* finished with file */
			fclose(srcf);
			/* insert (unique) tags into taglist */
			insert_tags(&tags, recipe->tags);
			/* insert recipe title and url into recipe list */
			insert_recipe(&recipes, recipe, slug);
			/* write recipe html file */
			write_recipe(dstf, recipe);
			fclose(dstf);
			recipe = NULL;  /* not heap allocated */
		}
		closedir(srcdir);
	} else {
		fprintf(stderr, "could not open source directory: %s\n.", src);
		return EXIT_FAILURE;
	}
	logprint("%sfinished%s: %lu recipes\n",
		ansi(BOLD), ansi(RESET), recipecount);
	/* write index.html file */
	logprint("%sgenerating%s: %s/index.html\n",
		ansi(BOLD), ansi(RESET), dst);
	write_index(dst, tags, recipes);
	/* write all tag files */
	logprint("%sgenerating%s: %lu tags filters\n",
		ansi(BOLD), ansi(RESET), tagcount);
	write_tagfiles(dst, tags, recipes);

	return EXIT_SUCCESS;
}

int
main(int argc, char **argv)
{
	int j, i = 0, err;
	size_t pagecount;
	clock_t t;
	char *src = (char *)ARTICLES_MARKDOWN;
	char *dst = (char *)ARTICLES_HTML;
	char *prog = argv[i++];

	for (j = i; i < argc; ++i, j = i) if (argv[i][0] == '-') {
		switch (argv[i][1]) {
		case '\0':
			fputs("stdin input not implemented.\n", stderr);
			return EXIT_FAILURE;
		case 'h':
			usage(prog);
			return EXIT_SUCCESS;
		case 's':
			src = argv[++i];
			break;
		case 'd':
			dst = argv[++i];
			break;
		case 'q':
			verbosity = 0;
			break;
		case 'C':
			/* clean build, ignore cache file */
			/* TODO */
		default:
			fprintf(stderr, "unknown option: -%c.\n", argv[i][1]);
			usage(prog);
			return EXIT_FAILURE;
		}
		if (argv[j][2] != '\0') {
			fprintf(stderr, "combining options in one argument is not supported.\n");
			fprintf(stderr, "separate '%c' to its own argument.\n", argv[j][2]);
			return EXIT_FAILURE;
		}
	} else {
		fprintf(stderr, "expected option, not \"%s\".\n", argv[i]);
		return EXIT_FAILURE;
	}

	/* you need to find and set a locale which actually understands diacritic
	 * and other basic stuff. setting LC_COLLATE=C will do a poor job of
	 * alphabetically sorting the recipe entries,
	 * i.e "Älplermagronen" ends up last, despite starting with 'Ä'.
	 */
	setlocale(LC_COLLATE, "en_US.UTF-8");

	t = clock();
	err = generate(src, dst);
	t = clock() - t;
	if (err != EXIT_SUCCESS) return err;

	/* fin. */
	pagecount = recipecount + tagcount + 1;
	logprint("--\n%sdone:%s generated %lu pages in %.1f milliseconds.\n",
		ansi(BOLD), ansi(RESET), pagecount, 1000 * (double)t / CLOCKS_PER_SEC);

	return EXIT_SUCCESS;
}
