/* config.h */
#ifndef _CONFIG_H
#define _CONFIG_H
/* enabling Git integration (recipe author, date posted, date edited, &c.)
 * adds about 6 seconds to build time (not cached), compared to a nearly
 * instant build time without it.
 */
#define GIT_INTEGRATION 1

/* fmt: unsigned int page_number */
#define FMT_PAGE_FILE "page-%u.html"

static const char FMT_RFC2822[] = "%a, %d %b %Y %T %z";
/* general formatting variables, all html is contained here */
static const char PAGE_TITLE[] = "Based Cooking";
static const char GIT_PATH[] = "/usr/bin/git";
static const char PAGE_DATE_FORMAT[] = "%F";
static const char PAGE_URL_ROOT[] = "https://based.cooking";
static const char INDEX_MARKDOWN[]    = "./index.md";
static const char ARTICLES_MARKDOWN[] = "./src";
static const char ARTICLES_HTML[]     = "./blog";
static const char CACHE_FILE[] = "./.buildcache";
static const char RSS_FILE[]  = "rss.xml";
static const char ATOM_FILE[] = "atom.xml";
static const unsigned RECIPES_PER_PAGE = 50;
static const char DESCRIPTION[] = {
	"Only Based cooking. "
	"No ads, no tracking, nothing but based cooking."
};
static const char CATEGORY[] = "cooking";
static const char FAVICON[] = {
	"data:image/svg+xml,"
	"<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22>"
	"<text y=%221em%22 font-size=%2280%22>🍲</text>"
	"</svg>"
};

/* fmt: char *page_title; char *desc; char *favicon */
static const char FMT_HTML_HEAD[] = {
	"<!DOCTYPE html>\n"
	"<html lang=\"en\">\n"
	"<head>\n"
	"	<meta charset=\"UTF-8\">\n"
	"	<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
	"	<title>%s</title>\n"
	"	<meta name=\"description\" content=\"%s\">\n"
	"	<link rel=\"icon\" href=\"%s\">\n"
    "	<link rel=\"stylesheet\" href=\"./style.css\">\n"
};

static const char FMT_HTML_PAGINATE_HEAD[] = {
	"	<link rel=\"stylesheet\" href=\"./page.css\">\n"
};

/* fmt: char *title */
static const char FMT_HTML_BANNER[] = {
	"	<div class=\"banner\">\n"
	"		<h1>🍲 %s 🍳</h1>\n"
	"		<hr />\n"
	"	</div>\n"
};

/* fmt: char *desc */
static const char FMT_HTML_INDEX_HEADER[] = {
	"	<p align=\"center\">%s</p>\n"
	"	<p><i>Tags:\n"
};

/* fmt: char *tag */
static const char FMT_HTML_TAG_HEADER[] = {
	"	<p><i>Filtering recipes tagged: <b>%s</b>\n"
};

static const char FMT_HTML_ARTICLE_HEADER[] = {
	"	<main>\n"
};

/* fmt: char *tag_url; char *tag_name */
static const char FMT_HTML_TAG_ENTRY[] = {
	"		<a href=\"@%s.html\">%s</a>"
};
static const char FMT_HTML_TAG_SEP[]   = { ", \n" };

static const char FMT_HTML_INDEX_LIST_START[] = {
	"	</i></p>\n"             /* close off tags list */
	"	<h2>Recipes</h2>\n"
	"	<ul id=\"artlist\">\n"
};

/* fmt: char *url; char *title */
static const char FMT_HTML_INDEX_LIST_ENTRY[] = {
	"		<li>\n"
	"			<a target=\"_parent\" href=\"%s\">%s</a>\n"
	"		</li>\n"
};

/* fmt: unsigned int start_page */
static const char FMT_HTML_INDEX_PAGINATOR[] = {
	"	</i></p>\n"             /* close off tags list */
	"	<h2>Recipes</h2>\n"
	"	<iframe\n"
	"		id=\"paginator\"\n"
	"		title=\"Recipes\"\n"
	"		frameborder=\"0\"\n"
	"		scrolling=\"no\"\n"
	"		src=\"./" FMT_PAGE_FILE "\">\n"
	"	</iframe>\n"
};


static const char FMT_HTML_PAGINATE_BAR_START[] = {
	"	<div id=\"bar\">\n"
};
/* fmt: unsigned int page_number; char letter */
static const char FMT_HTML_PAGINATE_BAR_LINK[] = {
	"		<a href=\"./" FMT_PAGE_FILE "\">[%c]</a>\n"
};
static const char FMT_HTML_PAGINATE_BAR_END[] = {
	"	</div>\n"
};

static const char FMT_HTML_PAGINATE_LIST_START[] = {
	"	<ul id=\"artlist\">\n"
};

/* alphabetical heading.
 * fmt: char alphabetic */
static const char FMT_HTML_PAGINATE_HEADER[] = {
	"		<span><h3>%c</h3></span>\n"
};

static const char FMT_HTML_PAGINATE_LIST_END[] = {
	"	</ul>\n"
};

static const char FMT_HTML_INDEX_LIST_END[] = {
	"	</ul>\n"
};

static const char FMT_HTML_PAGINATE_PAGE_LINKS_START[] = {
	"	<div id=\"pagelinks\">\n"
};

static const char FMT_HTML_PAGINATE_PAGE_LINKS_END[] = {
	"	</div>\n"
};

/* fmt: unsigned int page_number, page_number */
static const char FMT_HTML_PAGINATE_PAGE_LINK[] = {
	"		<a href=\"./" FMT_PAGE_FILE "\">[%u]</a>\n"
};

static const char FMT_HTML_PAGINATE_BUTTONS_START[] = {
	"	<div id=\"pagebuttons\">\n"
};

static const char FMT_HTML_PAGINATE_BUTTONS_END[] = {
	"	</div>\n"
};

/* fmt: unsigned int page_number, page_number */
static const char FMT_HTML_PAGINATE_CURRENT_PAGE[] = {
	"		<a id=\"thispage\" href=\"./" FMT_PAGE_FILE "\">[%u]</a>\n"
};

/* fmt: unsigned int page_number */
static const char FMT_HTML_PAGINATE_FIRST_BUTTON[] = {
	"		<a href=\"./" FMT_PAGE_FILE "\"><button>&lt;&lt;</button></a>\n"
};

/* fmt: unsigned int page_number */
static const char FMT_HTML_PAGINATE_BACK_BUTTON[] = {
	"		<a href=\"./" FMT_PAGE_FILE "\"><button>&lt;</button></a>\n"
};

/* fmt: unsigned int page_number */
static const char FMT_HTML_PAGINATE_NEXT_BUTTON[] = {
	"		<a href=\"./" FMT_PAGE_FILE "\"><button>&gt;</button></a>\n"
};

/* fmt: unsigned int page_number */
static const char FMT_HTML_PAGINATE_LAST_BUTTON[] = {
	"		<a href=\"./" FMT_PAGE_FILE "\"><button>&gt;&gt;</button></a>\n"
};

static const char FMT_HTML_ARTICLE_END[] = {
	"	<p><i>Recipe tags:\n"
};

/* fmt: char *date_posted; char *date_edited; char *git_author */
static const char FMT_HTML_ARTICLE_FOOTER[] = {
	"\n	</i></p>\n"  /* close recipe tags */
	"	</main>\n"
	"	<p><i>Recipe posted on: %s, last edited on: %s, written by: %s</i></p>\n"
};

static const char FMT_HTML_FOOTER[] = {
	"	<footer>\n"
    "		<hr />\n"
    "		<a href=\".\">homepage</a>\n"
    "		<a href=\"./rss.xml\">RSS</a>\n"
    "		<a href=\"./atom.xml\">atom</a>\n"
    "		<br />\n"
    "		<p>All site content is in the Public Domain.</p>\n"
	"	</footer>\n"
};

/* general constants & macros */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE
#define __USE_XOPEN
#define _GNU_SOURCE

#define PATH_LEN 256
#define SLUG_LEN 128
/* currently we have 249 recipes
 * increase MAX_RECIPES if we exceed that. */
#define MAX_RECIPES 400
/* currently we have 129 (!) tags.
 * increase MAX_TAGS if we exceed that. */
#define MAX_TAGS 200
/* TITLE_LEN: maximum length (bytes) for a recipe title. */
#define TITLE_LEN 64
/* TAG_COUNT: maximum number of tags on one recipe.
 * TAG_NAME_LEN: maximum number of characters in tag's name. */
#define TAG_COUNT 10
#define TAG_NAME_LEN 30
/* => 10 * 30 = 300 bytes (max) for tags */

#endif
