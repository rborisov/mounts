#ifndef __DB_UTILS_H__
#define __DB_UTILS_H__

int db_init();
void db_close();
char* db_get_song_album(char*, char*);
int db_get_prior_song_by_rating_first();
int db_get_song_by_rating_next();
char *db_get_song_name(int id);
char *db_get_song_artist(int id);

#endif
