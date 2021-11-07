#include "config.h"
#include "md.h"

struct cache {
	FILE *file;
	char filename[PATH_LEN];
	size_t size, count;
	struct md *entries;
};

void init_cache(struct cache *, char *);
void parse_cache(struct cache *);
struct md *next_cache(struct cache *);
struct md *insert_cache(struct cache *, struct md *);
struct md *update_cache(struct cache *, struct md *);
void dump_cache(struct cache *);
