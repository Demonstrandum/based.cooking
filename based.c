/* compiles recipe files to static site. */
#include "config.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <locale.h>
#include <assert.h>
/* looping through directories */
#include <dirent.h>
/* parsing markdown + tags */
#include "md.h"
/* writing rss + atom files */
#include "rss.h"
/* caching */
#if GIT_INTEGRATION
#include "cache.h"
#endif

#include "based.h"

void usage(char *prog)
{
	fprintf(stderr, "usage: %s [-hqC] [-s <src-dir>] [-d <dest-dir>] [-c <cache-file>]\n", prog);
	fprintf(stderr, "  -h	print help (this usage message).\n");
	fprintf(stderr, "  -s	(default: %s) specify source (markdown) directory.\n", ARTICLES_MARKDOWN);
	fprintf(stderr, "  -d	(default: %s) specify destination (html) directory.\n", ARTICLES_HTML);
	fprintf(stderr, "  -c	(default: %s) specify cache file.\n", CACHE_FILE);
	fprintf(stderr, "  -q	be quiet (no logging to stdout or stderr).\n");
	fprintf(stderr, "  -C	clean build (ignore cache file).\n");
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
	char indexfile[PATH_LEN];
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
	mkd_cleanup(mmio);
	fclose(mdf);
	/* end index document */
	fprintf(f, FMT_HTML_FOOTER);
	fprintf(f, "</body>\n</html>\n");
	fclose(f);

	return EXIT_SUCCESS;
}

#if GIT_INTEGRATION
/* recipes display author information as well as upload + edit times.
 * this is achieved by checking the git commit history for the file.
 */
static char date_format[50] = "--date=rfc";
static char date_formatter[] = "--pretty=format:%ad";
static char name_formatter[] = "--pretty=format:%an";
static char *git_env[] = { "GIT_PAGER=cat", "PAGER=cat", (char *)0 };
static char *git_log_added[] = {
	"--no-pager", "log", "-n", "1", "--diff-filter=A",
	/*[5]*/ NULL, /*[6]*/ NULL, "--", /*[8]*/ NULL,
	(char *)0
};
static char *git_log_modified[] = {
	"--no-pager", "log", "-n", "1", "--diff-filter=M",
	/*[5]*/ NULL, /*[6]*/ NULL, "--", /*[8]*/ NULL,
	(char *)0
};

static void
git_command(char *output, ssize_t size, char *src, char *formatter, char *arguments[])
{
	pid_t pid;
	int link[2];

	arguments[5] = date_format;
	arguments[6] = formatter;
	arguments[8] = src;

	if (0 != pipe(link)) die("pipe failed");
	pid = fork();
	if (pid == 0) {
		dup2(link[1], fileno(stdout));
		close(link[0]); close(link[1]);
		execve(GIT_PATH, arguments, git_env);
		logprint("git command failed\n");
		exit(1);
	} else if (pid > 0) {
		close(link[1]);
		read(link[0], output, size);
		close(link[0]);
		wait(NULL);
	} else {
		logprint("fork() failed\n");
		exit(1);
	}
}
#endif

static int
write_recipe(FILE *f, char *srcdir, struct md *recipe, bool modified)
{
	char (*tag)[TAG_NAME_LEN];
	char title[TITLE_LEN + sizeof(PAGE_TITLE) + 10] = { 0 };
#if GIT_INTEGRATION
	char adate[16] = { 0 };
	char mdate[16] = { 0 };
	char src[PATH_LEN];
#else
	(void)srcdir; (void)modified;
#endif

	sprintf(title, "%s – %s", recipe->title, PAGE_TITLE);
	fprintf(f, FMT_HTML_HEAD, title, DESCRIPTION, FAVICON);
	fprintf(f, "<body>\n");
	fprintf(f, FMT_HTML_ARTICLE_HEADER);
	fprintf(f, "%s", recipe->html);
	fprintf(f, FMT_HTML_ARTICLE_END);
	/* loop over recipe tags */
	for (tag = &recipe->tags[0]; (*tag)[0] != '\0'; ++tag) {
		fprintf(f, FMT_HTML_TAG_ENTRY, *tag, *tag);
		if (tag[1][0] != '\0') fprintf(f, FMT_HTML_TAG_SEP);
	}

#if GIT_INTEGRATION
	/* call git for author name, date posted & date edited */
	sprintf(src, "%s/%s.md", srcdir, recipe->slug);
	sprintf(date_format, "--date=format:%s", FMT_RFC2822);
	if (recipe->adate[0] == '\0')
		git_command(recipe->adate,  32, src, date_formatter, git_log_added);
	if (modified)
		git_command(recipe->mdate,  32, src, date_formatter, git_log_modified);
	if (recipe->mdate[0] == '\0')
		strcpy(recipe->mdate, recipe->adate);
	if (recipe->author[0] == '\0')
		git_command(recipe->author, 32, src, name_formatter, git_log_added);
	/* add to footer */
	fprintf(f, FMT_HTML_ARTICLE_FOOTER,
		from_rfc2822(PAGE_DATE_FORMAT, adate, 16, recipe->adate),
		from_rfc2822(PAGE_DATE_FORMAT, mdate, 16, recipe->mdate),
		recipe->author);
#endif

	fprintf(f, FMT_HTML_FOOTER);
	fprintf(f, "</body>\n</html>\n");

	return EXIT_SUCCESS;
}

static int
write_tagfiles(char *dst, struct taglist *tags, struct recipelist *recipes)
{
	FILE *f;
	char (*rtag)[TAG_NAME_LEN];
	char tagfile[PATH_LEN];
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
		for (rtag = &recipe->tags[0]; (*rtag)[0] != '\0'; ++rtag) {
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
slugsort(const struct dirent **_a, const struct dirent **_b)
{
	int cmp;
	char *a = (char *)(*_a)->d_name;
	char *b = (char *)(*_b)->d_name;
	size_t alen = strlen(a), blen = strlen(b);
	char olda = a[alen - 3];
	char oldb = b[blen - 3];

	a[alen - 3] = '\0';
	b[blen - 3] = '\0';
	cmp = -strcoll(a, b);
	a[alen - 3] = olda;
	b[blen - 3] = oldb;

	return cmp;
}

#if GIT_INTEGRATION
/* cache structure (i couldn't think of any other name) */
static struct cache hoard = { 0 };
#endif

static int
generate(char *src, char *dst, char *cachefile)
{
	FILE *dstf, *rssf, *atomf;
	struct dirent **sources;
	int entries;
	/* file names */
	char *slug;
	char srcfile[PATH_LEN + 8] = { '\0' };  /* `+ 8` not necissary but makes gcc shut up */
	char dstfile[PATH_LEN + 8] = { '\0' };
	char rssfile[PATH_LEN]  = { '\0' };
	char atomfile[PATH_LEN] = { '\0' };
	/* contains html and metadata (i.e. tags) */
	struct md *recipe;  /* parsed recipe */
#if GIT_INTEGRATION
	struct md *cached;  /* cahced recipe */
	struct stat srcstat;
	bool is_cached, modified, dst_exists;
#endif
	char (*tag)[TAG_NAME_LEN];
	/* linked list of alphabetically sorted tags */
	struct taglist *tags = NULL;
	/* linked list of alphabetically sorted titles */
	struct recipelist *recipes = NULL;

	/* initialise rss and atom files */
	sprintf(rssfile, "%s/%s", dst, RSS_FILE);
	sprintf(atomfile, "%s/%s", dst, ATOM_FILE);
	rssf = fopen(rssfile, "w");
	atomf = fopen(atomfile, "w");
	if (NULL == rssf)  die("failed to open %s for writing.", rssfile);
	if (NULL == atomf) die("failed to open %s for writing.", rssfile);
	write_rss_init(rssf);
	write_atom_init(atomf);

#if GIT_INTEGRATION
	/* initialise cache structure */
	init_cache(&hoard, cachefile);
	parse_cache(&hoard);
#else
	(void)cachefile;
#endif

	entries = scandir(src, &sources, NULL, slugsort);
	if (-1 == entries)
		die("could not open source directory: %s\n.", src);

	while (0 != entries--) {
		if (sources[entries]->d_name[0] == '.')
			continue;  /* skip filenames starting with '.' */
		slug = sources[entries]->d_name;
		/* trim `.md` off */
		slug[strlen(slug) - 3] = '\0';
		sprintf(srcfile, "%s/%s.md",   src, slug);
		sprintf(dstfile, "%s/%s.html", dst, slug);

#if GIT_INTEGRATION
		/* check cache is lined up */
		cached = next_cache(&hoard);
		is_cached = cached != NULL && 0 == strcmp(slug, cached->slug);
		/* compare timestamps */
		stat(srcfile, &srcstat);
		modified = is_cached && srcstat.st_mtime != cached->mtime;

		if (is_cached && !modified) {
			logprint("%sloaded cache%s: %s\n",
				ansi(BOLD), ansi(RESET), slug);
		} else {
			logprint("%s%sgenerating%s: %s -> %s\n",
				ansi(BOLD), modified ? "re-" : "", ansi(RESET), srcfile, dstfile);
		}
#else
		logprint("%sgenerating%s: %s -> %s\n",
			ansi(BOLD), ansi(RESET), srcfile, dstfile);
#endif

		/* convert md to html */
		recipe = mdparse(src, slug);
		logprint("  ├─ title: ‘%s’\n", recipe->title);
		logprint("  ╰── tags: ");
		for (tag = &recipe->tags[0]; (*tag)[0] != '\0'; ++tag)
			logprint("%s%s", *tag, tag[1][0] == '\0' ? "\n" : ", ");
		/* insert (unique) tags into taglist */
		insert_tags(&tags, recipe->tags);
		/* insert recipe title and url into recipe list */
		insert_recipe(&recipes, recipe, slug);

#if GIT_INTEGRATION
		recipe->mtime = srcstat.st_mtime;
		if (is_cached) {
			/* fields that should never change, so are always valid */
			strcpy(recipe->adate, cached->adate);
			strcpy(recipe->author, cached->author);
			if (!modified) {
				/* only valid if not modified */
				strcpy(recipe->mdate, cached->mdate);
			}
		}
		/* write recipe html file */
		dst_exists = 0 == access(dstfile, F_OK);
		if (!dst_exists || !is_cached || modified) {
			dstf = fopen(dstfile, "w");
			if (NULL == dstf) die("error opening %s.", dstfile);
			if (is_cached && !dst_exists && !modified) {
				/* is cached, but dstfile doesn't exist, nor was it modified */
				cached->html = recipe->html;
				write_recipe(dstf, src, cached, false);
				cached->html = NULL;
			} else {
				/* either not cached (new), or cached but source was modified */
				assert(!is_cached || modified);
				write_recipe(dstf, src, recipe, true);
				/* insert or overwrite into cache */
				if (modified) update_cache(&hoard, recipe);
				else          insert_cache(&hoard, recipe);
			}
			fclose(dstf);
		}
#else
		/* write recipe html file */
		dstf = fopen(dstfile, "w");
		if (NULL == dstf) die("error opening %s.", dstfile);
		write_recipe(dstf, src, recipe, true);
		fclose(dstf);
#endif
		/* write recipe rss fragment */
		write_rss_entry(rssf, recipe);
		/* write recipe atom fragment */
		write_atom_entry(atomf, recipe);
		mkd_cleanup(_mmio);  /* frees recipe->html */
		recipe = NULL;  /* not heap allocated */
	}
	free(sources);

	logprint("%sfinished%s: %lu recipes\n",
		ansi(BOLD), ansi(RESET), recipecount);
#if GIT_INTEGRATION
	/* finish and dump cache */
	dump_cache(&hoard);
	logprint("%sfinished%s: cache rebuilt\n", ansi(BOLD), ansi(RESET));
#endif
	/* finish rss file */
	write_rss_end(rssf);
	fclose(rssf);
	logprint("%sfinished%s: %s file\n",
		ansi(BOLD), ansi(RESET), rssfile);
	/* finish atom file */
	write_atom_end(atomf);
	fclose(atomf);
	logprint("%sfinished%s: %s file\n",
		ansi(BOLD), ansi(RESET), atomfile);

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
	struct timespec tic, toc;
	double timetaken;
	char *src = (char *)ARTICLES_MARKDOWN;
	char *dst = (char *)ARTICLES_HTML;
	char *cachefile = (char *)CACHE_FILE;
	char *prog = argv[i++];

	for (j = i; i < argc; ++i, j = i) if (argv[i][0] == '-') {
		switch (argv[i][1]) {
		case '\0':
			fputs("output to stdout not implemented.\n", stderr);
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
		case 'c':
			cachefile = argv[++i];
			break;
		case 'C':
			/* clean build, ignore cache file */
			/* TODO */
			fprintf(stderr, "clean building not implemented.\n");
			exit(EXIT_FAILURE);
			break;
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

	/* you need to find and set a locale which understands diacritics
	 * and other basic stuff. setting LC_COLLATE=C will do a poor job of
	 * alphabetically sorting the recipe entries,
	 * i.e "Älplermagronen" ends up last, despite starting with 'Ä'.
	 */
	setlocale(LC_COLLATE, "en_US.UTF-8");

	/* start clock on generate() function */
	clock_gettime(CLOCK_MONOTONIC, &tic);
	err = generate(src, dst, cachefile);
	if (err != EXIT_SUCCESS) return err;
	clock_gettime(CLOCK_MONOTONIC, &toc);

	/* fin. */
	timetaken = ( toc.tv_sec -  tic.tv_sec) * 1000.0
	          + (toc.tv_nsec - tic.tv_nsec) / 1000000.0;
	pagecount = recipecount + tagcount + 3;
	logprint("--\n%sdone:%s generated %lu pages in %.1f milliseconds.\n",
		ansi(BOLD), ansi(RESET), pagecount, timetaken);

	return EXIT_SUCCESS;
}

void
die(char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fputc('\n', stderr);
	if (errno) fprintf(stderr, " -> %s\n", strerror(errno));

	exit(errno || EXIT_FAILURE);
}

char *
from_rfc2822(const char *fmt, char *dst, size_t n, const char *src)
{
	struct tm tm = { 0 };
	strptime(src, FMT_RFC2822, &tm);
	strftime(dst, n, fmt, &tm);
	return dst;
}

char *
rfc3339time(char *dst, struct tm *tm)
{
	char tz[6] = { 0 };
	size_t n;

	n = strftime(dst, 26, "%Y-%m-%dT%H:%M:%S", tm);
	if (tm->tm_gmtoff == 0) sprintf(dst + n, "Z");
	else {
		/* add colon to timezone hh:mm */
		strftime(tz, 6, "%z", tm);
		sprintf(dst + n, "%c%c%c:%c%c",
			/* ± */ tz[0],
			/* h */ tz[1], /* h */ tz[2],
			/* m */ tz[3], /* m */ tz[4]);
	}
	return dst;
}

char *
to_rfc3339(char *dst, const char *src, const char *fmt)
{
	struct tm tm = { 0 };
	strptime(src, fmt, &tm);
	return rfc3339time(dst, &tm);
}
