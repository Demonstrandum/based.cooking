#include "rss.h"
#include "config.h"

static void
xmlescape(char *dst, char *src)
{
	for (;*src != '\0'; ++src)
		switch (*src) {
		case '&':  dst += sprintf(dst,   "&amp;"); break;
		case '<':  dst += sprintf(dst,    "&lt;"); break;
		case '>':  dst += sprintf(dst,    "&gt;"); break;
		case '"':  dst += sprintf(dst,  "&quot;"); break;
		case '\'': dst += sprintf(dst,  "&apos;"); break;
		default:   *dst++ = *src;
		}
}

void write_rss_init(FILE *f)
{
	fprintf(f, "<rss version=\"2.0\">\n");
	fprintf(f, "<channel>\n");
	fprintf(f, "	<title>%s</title>\n", PAGE_TITLE);
	fprintf(f, "	<link>%s</link>\n", PAGE_URL_ROOT);
	fprintf(f, "	<description>%s</description>\n", DESCRIPTION);
	fprintf(f, "	<category>%s</category>\n", CATEGORY);
}

void write_atom_init(FILE *f)
{
	(void)f;
}

void write_rss_entry(FILE *f, struct md *recipe)
{
	char (*tag)[TAG_NAME_LEN] = &(recipe->tags[0]);
	char title[TITLE_LEN + 16] = { 0 };
	xmlescape(title, recipe->title);

	fprintf(f, "	<item>\n");
	fprintf(f, "		<title>%s</title>\n", title);
	fprintf(f, "		<link>%s/%s.html</link>\n", PAGE_URL_ROOT, recipe->slug);
	fprintf(f, "		<guid>%s</guid>\n", recipe->slug);
#ifdef GIT_INTEGRATION
	fprintf(f, "		<author>%s</author>\n", recipe->author);
	fprintf(f, "		<pubDate>%s</pubDate>\n", recipe->published);
#endif
	fprintf(f, "		<category>");
	for (; tag[0][0] != '\0'; ++tag)
		fprintf(f, "%s%s", tag[0], tag[1][0] == '\0' ? "</category>\n" : " ");
	fprintf(f, "		<description>\n");
	fprintf(f, "			<![CDATA[%s]]>\n", recipe->html);
	fprintf(f, "		</description>\n");
	fprintf(f, "	</item>\n");
}

void write_atom_entry(FILE *f, struct md *recipe)
{
	(void)f; (void)recipe;
}

void write_rss_end(FILE *f)
{
	fprintf(f, "</channel>");
	fprintf(f, "</rss>\n");
}

void write_atom_end(FILE *f)
{
	(void)f;
}
