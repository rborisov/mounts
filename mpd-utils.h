#ifndef _MPD_UTILS_H_
#define _MPD_UTILS_H_

#include <mpd/client.h>

struct mpd_connection * setup_connection(void);
int update_mpd_db(char *song);
int isin_mpd_db(char *artist, char *title);
int mpd_search_and_delete(char *artist, char * title);

#endif
