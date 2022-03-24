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
#include <iconv.h>
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

/* returns an alphabetic character to which the string belongs */
static char
alphord(char *str)
{
	char out[2] = { 0 };
	char *ptr = out;
	size_t inlen = strlen(str), outlen = 1;
	iconv_t cd;

	cd = iconv_open("ASCII//TRANSLIT", "UTF-8");
	iconv(cd, &str, &inlen, &ptr, &outlen);
	iconv_close(cd);

	return out[0];
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

static unsigned  /* ceil division */
atleast(unsigned n, unsigned d) { return n / d + (n % d != 0); }

struct letter_on_page {
	char letter;
	unsigned page;
};

/* pages for paginator */
static int
write_pages(char *dst, struct recipelist *recipe)
{
	FILE *pagef;
	char pagefile[PATH_LEN];
	struct recipelist *first, *last;  /* recipe before current recipe */
	unsigned page, pages, count, lettercount;
	bool open = false;
	struct letter_on_page *letterpages, *letterpage;

	first = last = recipe;
	pages = atleast(recipecount, RECIPES_PER_PAGE);

	/* initial indexing of recipes according to alphabet */
	lettercount = 0;
	letterpages = calloc(30, sizeof(struct letter_on_page));
	letterpages[lettercount++] =
		(struct letter_on_page){ alphord(recipe->title), 1 };
	for (page = 1; page <= pages; ++page) {
		for (count = 0; count < RECIPES_PER_PAGE && recipe != NULL;
				last = recipe, recipe = recipe->next, ++count) {
			if (alphord(last->title) == alphord(recipe->title))
				continue;
			letterpages[lettercount++] =
				(struct letter_on_page){ alphord(recipe->title), page };
		}
	}

	last = recipe = first;
	for (page = 1; page <= pages; ++page) {
		sprintf(pagefile, "%s/"FMT_PAGE_FILE, dst, page);
		pagef = fopen(pagefile, "w");
		if (NULL == pagef) die("failed to open page %u for writing.", page);
		/* each page needs full valid HTML */
		fprintf(pagef, FMT_HTML_HEAD, PAGE_TITLE, DESCRIPTION, FAVICON);
		fprintf(pagef, FMT_HTML_PAGINATE_HEAD);
		fprintf(pagef, "</head>\n<body>\n");

		/* write paginator alphabet bar */
		fprintf(pagef, FMT_HTML_PAGINATE_BAR_START);
		if (page != 1)
			fprintf(pagef, "<span>");
		for (count = 0, letterpage = letterpages;
			 count < lettercount;
			 ++count, ++letterpage) {
			/* grey-out 'active' letters for page */
			if (letterpage->page == page && !open) {
				open = true;
				if (page != 1)
					fprintf(pagef, "</span>");
				fprintf(pagef, "<span id=\"active\">");
			}
			if (letterpage->page != page && open) {
				open = false;
				fprintf(pagef, "</span>");
				fprintf(pagef, "<span>");
			}
			fprintf(pagef, FMT_HTML_PAGINATE_BAR_LINK,
			        letterpage->page, letterpage->letter);
		}
		fprintf(pagef, "</span>");
		fprintf(pagef, FMT_HTML_PAGINATE_BAR_END);

		/* write recipe list entries with alphabet headers */
		fprintf(pagef, FMT_HTML_PAGINATE_LIST_START);
		fprintf(pagef, FMT_HTML_PAGINATE_HEADER, alphord(recipe->title));
		for (count = 0; count < RECIPES_PER_PAGE && recipe != NULL;
				last = recipe, recipe = recipe->next, ++count) {
			/* if first character of recipe title advanced in the alphabet,
			 * then print a new alphabetical heading */
			if (alphord(recipe->title) != alphord(last->title)) {
				fprintf(pagef, FMT_HTML_PAGINATE_HEADER, alphord(recipe->title));
			}
			fprintf(pagef, FMT_HTML_INDEX_LIST_ENTRY, recipe->url, recipe->title);
		}

		fprintf(pagef, FMT_HTML_PAGINATE_LIST_END);
		/* write page navigation controls */
		fprintf(pagef, "<nav>\n");
		/* write page links */
		fprintf(pagef, FMT_HTML_PAGINATE_PAGE_LINKS_START);
		for (count = 1; count <= pages; ++count)
			if (count != page)
				fprintf(pagef, FMT_HTML_PAGINATE_PAGE_LINK, count, count);
		fprintf(pagef, FMT_HTML_PAGINATE_PAGE_LINKS_END);
		/* write appropriate paginator buttons */
		fprintf(pagef, FMT_HTML_PAGINATE_BUTTONS_START);
		if (page != 1) {  /* no back button on first page */
			fprintf(pagef, FMT_HTML_PAGINATE_FIRST_BUTTON, 1);
			fprintf(pagef, FMT_HTML_PAGINATE_BACK_BUTTON, page - 1);
		}
		fprintf(pagef, FMT_HTML_PAGINATE_CURRENT_PAGE, page, page);
		if (page != pages) { /* no next button on last page */
			fprintf(pagef, FMT_HTML_PAGINATE_NEXT_BUTTON, page + 1);
			fprintf(pagef, FMT_HTML_PAGINATE_LAST_BUTTON, pages);
		}
		fprintf(pagef, FMT_HTML_PAGINATE_BUTTONS_END);
		fprintf(pagef, "</nav>\n");
		/* page finished */
		fprintf(pagef, "</body>\n</html>\n");
		fclose(pagef);
	}

	free(letterpages);
	return EXIT_SUCCESS;
}

static int
write_index(char *dst, struct taglist *tag, struct recipelist *recipe)
{
	FILE *f, *mdf;  /* index.html file, index.md file. */
	MMIOT *mmio;
	int res;
	char indexfile[PATH_LEN];
	sprintf(indexfile, "%s/index.html", dst);

	/* first write paginator pages */
	res = write_pages(dst, recipe);
	if (res != EXIT_SUCCESS) return res;

	f = fopen(indexfile, "w");
	fprintf(f, FMT_HTML_HEAD, PAGE_TITLE, DESCRIPTION, FAVICON);
	fprintf(f, "</head>\n<body>\n");
	fprintf(f, FMT_HTML_BANNER, PAGE_TITLE);
	fprintf(f, FMT_HTML_INDEX_HEADER, DESCRIPTION);
	/* loop through tags */
	for (; tag != NULL; tag = tag->next) {
		fprintf(f, FMT_HTML_TAG_ENTRY, tag->name, tag->name);
		if (tag->next != NULL) fprintf(f, FMT_HTML_TAG_SEP);
	}
	/* embed iframe to paginator */
	fprintf(f, FMT_HTML_INDEX_PAGINATOR, 1);
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
		fprintf(stderr, "error: git command failed\n");
		exit(1);
	} else if (pid > 0) {
		close(link[1]);
		if (-1 == read(link[0], output, size)) die("read failed");
		close(link[0]);
		wait(NULL);
	} else {
		fprintf(stderr, "error: fork() failed\n");
		exit(1);
	}
}
#endif

const char h2_ingredients[]  = "<h2>Ingredients</h2>";
const char h2_contribution[] = "<h2>Contrib";


/* allocates memory, must be freed,
 * or returns NULL when syntax is not used.
 * expands {metric,imperial} untis syntax in text
 * between "## Ingredients" and "## Contribution" sections.
 */
static char *
expand_units(char *html)
{
	char *expanded, *metric, *imperial;
	size_t ioff, moff;  /* imperial and metric pointer offset */
	size_t i;
	bool uses_syntax;
	size_t htmlsize;
	/* pointers to end of header and start of footer */
	char *header, *footer;

	htmlsize = strlen(html);
	metric = calloc(htmlsize, 1);
	imperial = calloc(htmlsize, 1);
	if (metric == NULL || imperial == NULL)
		die("could not allocate memory for recipe.");
	uses_syntax = false;
	header = footer = NULL;

	i = ioff = moff = 0;
	while ('\0' != html[i]) {
		if (NULL == header) {
			if (0 == strncmp(html + i, h2_ingredients, strlen(h2_ingredients))) {
				/* found ingredients header (<h2>) */
				i += strlen(h2_ingredients);
				header = html + i;
				continue;
			}
		} else if (NULL == footer) {
			if (0 == strncmp(html + i, h2_contribution, strlen(h2_contribution))) {
				/* found contribution footer (<h2>) */
				footer = html + i;
				i += strlen(h2_contribution);
				continue;
			}
			/* scan for special units syntax, */
			/* since header is not null, but footer is */
			assert(header != NULL && footer == NULL);
			/* parse line by line */
			while ('\n' != html[i] && '\0' != html[i]) {
				if ('{' == html[i]) {
					uses_syntax = true;
					/* parse syntax */
					assert('{' == html[i++]);  /* discard '{' */
					while (',' != html[i]) metric[moff++] = html[i++];
					assert(',' == html[i++]);  /* discard ',' */
					while ('}' != html[i]) imperial[ioff++] = html[i++];
					assert('}' == html[i++]);  /* discard '}' */
					continue;
				}
				/* otherwise, write to imperial and metric equally */
				imperial[ioff++] = metric[moff++] = html[i++];
			}
			imperial[ioff++] = metric[moff++] = html[i++];
			continue;
		}
		/* skip to next line */
		while ('\n' != html[i++]);
	}
	/* if we have no footer (contribution section),
	 * then set the footer pointer to the end of the html */
	if (NULL == footer) footer = html + i;

	expanded = NULL;  /* write to `expanded` if appropriate */
	if (header == NULL || footer == NULL) {
		fprintf(stderr, "%swarning%s: recipe missing important sections.\n",
			ansi(BOLD), ansi(RESET));
		if (header == NULL) fprintf(stderr, " · missing ingredients section.\n");
		if (footer == NULL) fprintf(stderr, " · missing contribution section.\n");
	} else if (uses_syntax) {
		/* write metric and imperial sections to `expanded` */
		assert(header != NULL && footer != NULL);
		expanded = calloc(htmlsize * 2, 1);
		if (NULL == expanded)
			die("could not allocate memory for recipe.");
		i = 0;  /* offset in `expanded` */
		/* copy header */
		strncpy(expanded + i, html, header - html);
		i += header - html;
		/* open metric details submenu and add content */
		i += sprintf(expanded + i,
			"<details class=\"units\" id=\"metric\">"
			"<summary>Metric Units</summary>\n");
		i += sprintf(expanded + i, "%s\n", metric);
		i += sprintf(expanded + i, "</details>\n");
		/* open imperial details tag and copy content */
		i += sprintf(expanded + i,
			"<details class=\"units\" id=\"imperial\" open>"
			"<summary>US Customary Units</summary>\n");
		i += sprintf(expanded + i, "%s\n", imperial);
		i += sprintf(expanded + i, "</details>\n");
		/* copy in footer */
		i += sprintf(expanded + i, "%s", footer);
		expanded[i] = '\0';
	}

	free(metric);
	free(imperial);
	return expanded;
}

static int
write_recipe(FILE *f, char *srcdir, struct md *recipe, bool modified)
{
	char (*tag)[TAG_NAME_LEN];
	char title[TITLE_LEN + sizeof(PAGE_TITLE) + 10] = { 0 };
	char *recipehtml;
#if GIT_INTEGRATION
	char adate[16] = { 0 };
	char mdate[16] = { 0 };
	char src[PATH_LEN];
#else
	(void)srcdir; (void)modified;
#endif

	sprintf(title, "%s – %s", recipe->title, PAGE_TITLE);
	fprintf(f, FMT_HTML_HEAD, title, DESCRIPTION, FAVICON);
	fprintf(f, "</head>\n<body>\n");
	fprintf(f, FMT_HTML_ARTICLE_HEADER);
	/* expand {metric,imperial} syntax into two sections */
	recipehtml = expand_units(recipe->html);
	if (NULL != recipehtml) {
		fprintf(f, "%s", recipehtml);
		free(recipehtml);
	} else {
		fprintf(f, "%s", recipe->html);
	}

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
		fprintf(f, "</head>\n<body>\n");
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
	setlocale(LC_ALL, "en_US.UTF-8");  /*< for iconv to transliterate */

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
