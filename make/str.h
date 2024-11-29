#ifndef FILE_STR_H
#define FILE_STR_H

typedef struct string {
	char *ptr;
	size_t len, cap;
} str_t;

extern str2_new (/* str_t * */);
extern str2_free (/* str_t * */);
extern str2_putc (/* str_t *, int */);
extern str2_puts (/* str_t *, char * */);
extern str2_write (/* str_t *, char *, size_t */);
extern str2_last (/* str_t * */);
extern str2_pop (/* str_t * */);
extern str2_chomp (/* str_t * */);
extern str2_trim (/* str_t * */);
extern str2_reset (/* str_t * */);
extern char *str2_release (/* str_t * */);

#define str2_get(s) ((s)->ptr)

#endif /* FILE_STR_H */
