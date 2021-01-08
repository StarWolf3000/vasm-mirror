/* listing.h - listing file */
/* (c) in 2020 by Volker Barthelmann */

#ifndef LISTING_H
#define LISTING_H

/* listing table */

#define MAXLISTSRC 120

struct listing {
  listing *next;
  source *src;
  int line;
  int error;
  atom *atom;
  section *sec;
  taddr pc;
  char txt[MAXLISTSRC];
};


extern int produce_listing;
extern int listena,listformfeed,listlinesperpage,listnosyms;
extern listing *first_listing,*last_listing,*cur_listing;

void set_listing(int);
void set_list_title(char *,int);
void write_listing(char *,section *);

#endif  /* LISTING_H */
