/* general formatting variables, all html is contained here */
static const char PAGE_TITLE[] = "Based Cooking";
static const char PAGE_DATE_FORMAT[] = "%F";
static const char PAGE_URL_ROOT[] = "https://based.cooking/";
static const char INDEX_MARKDOWN[]    = "./index.md";
static const char ARTICLES_MARKDOWN[] = "./src";
static const char ARTICLES_HTML[]     = "./blog";
static const char DESCRIPTION[] = {
	"Only Based cooking. "
	"No ads, no tracking, nothing but based cooking."
};
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
	"</head>\n"
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
	"	<p>%s</p>\n"
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
	"			<a href=\"%s\">%s</a>\n"
	"		</li>\n"
};

static const char FMT_HTML_INDEX_LIST_END[] = {
	"	</ul>\n"
};

static const char FMT_HTML_ARTICLE_END[] = {
	"<p><i>Recipe tags:"
};

/* fmt: char *date_posted; char *date_edited; char *git_author */
static const char FMT_HTML_ARTICLE_FOOTER[] = {
	"	</i></p>\n"  /* close recipe tags */
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
