/* parse md files. */
#include "md.h"
#include "config.h"
#include "based.h"
#include <string.h>
#include <errno.h>
/*
 * libmarkdown parser, also provides the `markdown` command.
 * you probably have this installed already. see `man 3 markdown`.
 */
#include <mkdio.h>

/*
 * LINE_LENGTH: maximum characters in a source file line.
 */
#define LINE_LENGTH 400
#define TAGS_PREFIX ";tags: "
#define FILE_BUF_SIZE (1 << 13)  /* ~8.2KiB */

struct md _parsed_md = { 0 };
MMIOT *_mmio = NULL;

struct md *
mdparse(char *srcdir, char *slug)
{
	FILE *f;
	char src[PATH_LEN];
	int i, tag_start, tag = 0;
	char linbuf[LINE_LENGTH] = { '\0' };
	size_t linlen = 0;
	int    doclen = 0;
	char filebuf[FILE_BUF_SIZE];

	/* important: null out old parse before reuse.
	 * we rely on NUL-bytes being in their appropriate places.
	 */
	memset(&_parsed_md, 0, sizeof(_parsed_md));

	/* open source file */
	sprintf(src, "%s/%s.md", srcdir, slug);
	f = fopen(src, "r");
	if (NULL == f) die("file was moved.");

	strcpy(_parsed_md.slug, slug);

	/* first write raw markdown into `filebuf`. */
	while (!feof(f)) {
		if (NULL == fgets(linbuf, sizeof(linbuf), f)) break;
		linlen = strlen(linbuf);
		/* parse tags, excluding them from the markdown */
		if (0 == strncmp(linbuf, TAGS_PREFIX, sizeof(TAGS_PREFIX) - 1)) {
			for (i = sizeof(TAGS_PREFIX) - 1, tag_start = i;
			     linbuf[i] != '\n' && linbuf[i] != '\0'; ++i) {
				if (linbuf[i + 1] == ' '
				 || linbuf[i + 1] == '\n'
				 || linbuf[i + 1] == '\0') {
					/* technically all Unix files should end in a linefeed */
					/* but apparently we can't rely on that fact. */
					memcpy(_parsed_md.tags[tag++], linbuf + tag_start, i - tag_start + 1);
					tag_start = i + 2;
				}
			}
		} else {
			/* copy the #/<h1> header title */
			if (0 == strncmp("# ", linbuf, 2) && _parsed_md.title[0] == '\0')
				memcpy(_parsed_md.title, linbuf + 2, strchr(linbuf, '\n') - linbuf - 2);
			/* seep up all left over markdown */
			memcpy(filebuf + doclen, linbuf, linlen);
			doclen += linlen;
		}
		/* important: null linbuf before reuse. */
		memset(&linbuf, 0, sizeof(linbuf));
	}

	/* file finished, now parse markdown */
	fclose(f);
	_mmio = mkd_string(filebuf, doclen, mkd_flags);
	if (_mmio == NULL) {
		fprintf(stderr, "error parsing markdown file. (%d):\n", errno);
		fprintf(stderr, "  %s\n", strerror(errno));
		exit(1);
	}
	mkd_compile(_mmio, mkd_flags);
	doclen = mkd_document(_mmio, &_parsed_md.html);
	if (_parsed_md.html == NULL) {
		fprintf(stderr, "error generating markdown file. (%d):\n", errno);
		fprintf(stderr, "  %s\n", strerror(errno));
		exit(1);
	}

	/* check metadata */
	if (_parsed_md.title[0] == '\0')
		fprintf(stderr, "warning: file has no header title (# ...).\n");

	return &_parsed_md;
}

void
string_from_tags(char *dst, char (*tags)[TAG_NAME_LEN])
{
	size_t ofs;
	for (ofs = 0; tags[0][0] != '\0'; ++tags)
		ofs += sprintf(dst + ofs, "%s ", tags[0]);
	dst[ofs - 1] = '\0';
}

void
tags_from_string(char (*dst)[TAG_NAME_LEN], char *tags)
{
	size_t i, j, c;

	for (c = i = j = 0; tags[i] != '\0'; ++i) {
		if (tags[i] == ' ') { ++c; j = 0; }
		else { dst[c][j++] = tags[i]; }
	}
}
