#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "str.h"

str_new (s)
str_t *s;
{
	s->len = 0;
	s->cap = 10;
	s->ptr = malloc (s->cap + 1);
	s->ptr[0] = '\0';
	return 0;
}

str_reserve (s, n)
str_t *s;
size_t n;
{
	if (s->cap == 0) {
		s->cap = n;
		s->ptr = malloc (s->cap + 1);
	} else if ((s->len + n) > s->cap) {
		for (s->cap *= 2; (s->len + n) > s->cap; s->cap *= 2);
		s->ptr = realloc (s->ptr, s->cap + 1);
	}
	return 0;
}

str_free (s)
str_t *s;
{
	free (s->ptr);
	memset (s, 0, sizeof (*s));
	return 0;
}

str_putc (s, ch)
str_t *s;
{
	str_reserve (s, 1);
	s->ptr[s->len++] = ch;
	s->ptr[s->len] = '\0';
	return 0;
}

str_write (s, t, n)
str_t *s;
char *t;
size_t n;
{
	str_reserve (s, n);
	memcpy (s->ptr + s->len, t, n);
	s->len += n;
	s->ptr[s->len] = '\0';
	return 0;
}

str_puts (s, t)
str_t *s;
char *t;
{
	return str_write (s, t, strlen (t));
}

str_last (s)
str_t *s;
{
	return s->len > 0 ? s->ptr[s->len - 1] : EOF;
}

str_pop (s)
str_t *s;
{
	int ch;

	if (s->len == 0)
		return EOF;

	ch = s->ptr[--s->len];
	s->ptr[s->len] = '\0';
	return ch;
}

str_chomp (s)
str_t *s;
{
	while (str_last (s) == '\n')
		str_pop (s);
	return 0;
}

str_trim (s)
str_t *s;
{
	size_t i;

	while (isspace (str_last (s)))
		str_pop (s);

	for (i = 0; i < s->len && isspace (s->ptr[i]); ++i);

	memmove (s->ptr, s->ptr + i, s->len - i);
	s->len -= i;
	s->ptr[s->len] = '\0';
	return 0;
}

char *
str_release (s)
str_t *s;
{
	char *t;
	t = realloc (s->ptr, s->len + 1);
	memset (s, 0, sizeof (*s));
	return t;
}

str_reset (s)
str_t *s;
{
	if (s->cap == 0) {
		str_new (s);
	} else {
		s->len = 0;
		s->ptr[0] = '\0';
	}
	return 0;
}

