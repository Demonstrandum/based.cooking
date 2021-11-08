#include "rss.h"
#include "config.h"
#include "based.h"

static void
xmlencode(char *dst, char *src)
{
	char semi, entity[5];
	for (;*src != '\0'; ++src) {
		if (2 == sscanf(src, "&%4[^;]%c", entity, &semi) && semi == ';') {
			/* leave already encoded entities as they are */
			dst += sprintf(dst, "&%s;", entity);
			src += strlen(entity) + 1;
			continue;
		}
		switch (*src) {
		case '&':  dst += sprintf(dst,   "&amp;"); break;
		case '<':  dst += sprintf(dst,    "&lt;"); break;
		case '>':  dst += sprintf(dst,    "&gt;"); break;
		case '"':  dst += sprintf(dst,  "&quot;"); break;
		case '\'': dst += sprintf(dst,  "&apos;"); break;
		default:   *dst++ = *src;
		}
	}
}

void write_rss_init(FILE *f)
{
	fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(f, "<rss version=\"2.0\">\n");
	fprintf(f, "<channel>\n");
	fprintf(f, "	<title>%s</title>\n", PAGE_TITLE);
	fprintf(f, "	<link>%s</link>\n", PAGE_URL_ROOT);
	fprintf(f, "	<description>%s</description>\n", DESCRIPTION);
	fprintf(f, "	<category>%s</category>\n", CATEGORY);
}

void write_atom_init(FILE *f)
{
	time_t today;
	struct tm *tm;
	char updated[26] = { 0 };

	time(&today);
	tm = localtime(&today);
	rfc3339time(updated, tm);

	fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(f, "<feed xmlns=\"http://www.w3.org/2005/Atom\" xml:lang=\"en\">\n");
	fprintf(f, "	<title type=\"text\">%s</title>\n", PAGE_TITLE);
	fprintf(f, "	<subtitle type=\"text\">%s</subtitle>\n", DESCRIPTION);
	fprintf(f, "	<category term=\"%s\"/>\n", CATEGORY);
	fprintf(f, "	<updated>%s</updated>\n", updated);
	fprintf(f, "	<link rel=\"alternate\" type=\"text/html\" href=\"%s/\"/>\n", PAGE_URL_ROOT);
	fprintf(f, "	<id>%s/%s</id>\n", PAGE_URL_ROOT, ATOM_FILE);
	fprintf(f, "	<link rel=\"self\" type=\"application/atom+xml\" href=\"%s/%s\"/>\n", PAGE_URL_ROOT, ATOM_FILE);
}

void write_rss_entry(FILE *f, struct md *recipe)
{
	char (*tag)[TAG_NAME_LEN] = &(recipe->tags[0]);
	char title[TITLE_LEN + 16] = { 0 };
	xmlencode(title, recipe->title);

	fprintf(f, "	<item>\n");
	fprintf(f, "		<title>%s</title>\n", title);
	fprintf(f, "		<link>%s/%s.html</link>\n", PAGE_URL_ROOT, recipe->slug);
	fprintf(f, "		<guid isPermaLink=\"true\">%s/%s.html</guid>\n", PAGE_URL_ROOT, recipe->slug);
#if GIT_INTEGRATION
	fprintf(f, "		<author>%s</author>\n", recipe->author);
	fprintf(f, "		<pubDate>%s</pubDate>\n", recipe->adate);
#endif
	for (; tag[0][0] != '\0'; ++tag)
		fprintf(f, "		<category>%s</category>\n", tag[0]);
	fprintf(f, "		<description>\n");
	fprintf(f, "			<![CDATA[%s]]>\n", recipe->html);
	fprintf(f, "		</description>\n");
	fprintf(f, "	</item>\n");
}

void write_atom_entry(FILE *f, struct md *recipe)
{
	char (*tag)[TAG_NAME_LEN] = &(recipe->tags[0]);
	char title[TITLE_LEN + 16] = { 0 };
#if GIT_INTEGRATION
	char publish[26] = { 0 };
	char updated[26] = { 0 };
	to_rfc3339(publish, recipe->adate, FMT_RFC2822);
	to_rfc3339(updated, recipe->mdate, FMT_RFC2822);
#endif

	xmlencode(title, recipe->title);
	fprintf(f, "	<entry>\n");
	fprintf(f, "		<title type=\"text\">%s</title>\n", title);
	fprintf(f, "		<link rel=\"alternate\" type=\"text/html\" href=\"%s/%s.html\"/>\n", PAGE_URL_ROOT, recipe->slug);
	fprintf(f, "		<id>%s/%s.html</id>\n", PAGE_URL_ROOT, recipe->slug);
#if GIT_INTEGRATION
	fprintf(f, "		<published>%s</published>\n", publish);
	fprintf(f, "		<updated>%s</updated>\n", updated);
	fprintf(f, "		<author><name>%s</name></author>\n", recipe->author);
#endif
	for (; tag[0][0] != '\0'; ++tag)
		fprintf(f, "		<category term=\"%s\" label=\"%s\"/>\n", *tag, *tag);
	fprintf(f, "		<summary type=\"html\">\n");
	fprintf(f, "			<![CDATA[%s]]>\n", recipe->html);
	fprintf(f, "		</summary>\n");
	fprintf(f, "	</entry>\n");
}

void write_rss_end(FILE *f)
{
	fprintf(f, "</channel>\n");
	fprintf(f, "</rss>\n");
}

void write_atom_end(FILE *f)
{
	fprintf(f, "</feed>\n");
}
