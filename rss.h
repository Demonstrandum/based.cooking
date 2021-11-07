/* writing rss + atom files */
#ifndef _RSS_H
#define _RSS_H

#include <stdio.h>
#include "md.h"

void write_rss_init(FILE *);
void write_atom_init(FILE *);

void write_rss_entry(FILE *, struct md *);
void write_atom_entry(FILE *, struct md *);

void write_rss_end(FILE *);
void write_atom_end(FILE *);

#endif
