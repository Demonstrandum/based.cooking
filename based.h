#ifndef TAG_NAME_LEN
#error "You need to #define the max TAG_NAME_LEN."
#endif

#ifndef TAG_COUNT
#error "You need to #define the max TAG_COUNT."
#endif

#ifndef TITLE_LEN
#error "You need to #define the max TITLE_LEN."
#endif

/* linked list of all tags alphabetically inserted. */
struct taglist {
	char name[TAG_NAME_LEN];
	struct taglist *next;
};

/* linked list of all titles alphabetically inserted. */
struct recipelist {
	char title[TITLE_LEN];
	char url[TITLE_LEN];
	char tags[TAG_COUNT][TAG_NAME_LEN];
	struct recipelist *next;
};
