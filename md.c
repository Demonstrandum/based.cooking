/* parse md files. */
#include "md.h"
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

struct md _parsed_md = { 0 };

struct md *
mdparse(FILE *f)
{
	int i, tag_start, tag = 0;
	char linbuf[LINE_LENGTH] = { '\0' };
	size_t linlen = 0;
	int    doclen = 0;
	char *html = _parsed_md.html;
	char *doc = NULL;
	MMIOT *mmio = NULL;

	/* important: null out old parse before reuse.
	 * we rely on NUL-bytes being in their appropriate places.
	 */
	memset(&_parsed_md, 0, sizeof(_parsed_md));

	/* first write raw markdown into `_parsed_md.html`. */
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
			memcpy(html + doclen, linbuf, linlen);
			doclen += linlen;
		}
		/* important: null linbuf before reuse. */
		memset(&linbuf, 0, sizeof(linbuf));
	}

	/* file finished, now parse markdown */
	mmio = mkd_string(html, doclen, mkd_flags);
	if (mmio == NULL) {
		fprintf(stderr, "error parsing markdown file. (%d):\n", errno);
		fprintf(stderr, "  %s\n", strerror(errno));
		exit(1);
	}
	mkd_compile(mmio, mkd_flags);
	doclen = mkd_document(mmio, &doc);
	if (doc == NULL) {
		fprintf(stderr, "error generating markdown file. (%d):\n", errno);
		fprintf(stderr, "  %s\n", strerror(errno));
		exit(1);
	}

	/* copy html into _parsed_md.html field. */
	memset(html, 0, HTML_LEN);
	memcpy(html, doc, doclen);
	/* check metadata */
	if (_parsed_md.title[0] == '\0')
		fprintf(stderr, "warning: file has no header title (# ...).\n");

	/* cleanup, frees `doc` too. */
	mkd_cleanup(mmio);

	return &_parsed_md;
}

