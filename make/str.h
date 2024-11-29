#ifndef FILE_STR_H
#define FILE_STR_H

typedef struct string {
	char *ptr;
	size_t len, cap;
} str_t;

extern str_new (/* str_t * */);
extern str_free (/* str_t * */);
extern str_putc (/* str_t *, int */);
extern str_puts (/* str_t *, char * */);
extern str_write (/* str_t *, char *, size_t */);
extern str_last (/* str_t * */);
extern str_pop (/* str_t * */);
extern str_chomp (/* str_t * */);
extern str_trim (/* str_t * */);
extern str_reset (/* str_t * */);
extern char *str_release (/* str_t * */);

#define str_get(s) ((s)->ptr)

#endif /* FILE_STR_H */
