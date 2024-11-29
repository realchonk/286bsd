#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "str.h"

str2_new (s)
str_t *s;
{
	s->cap = 10;
	s->len = 0;
	s->ptr = malloc (s->cap + 1);
	s->ptr[0] = '\0';
	return 0;
}

str2_reserve (s, n)
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

str2_free (s)
str_t *s;
{
	free (s->ptr);
	memset (s, 0, sizeof (*s));
	return 0;
}

str2_putc (s, ch)
str_t *s;
{
	str2_reserve (s, 1);
	s->ptr[s->len++] = ch;
	s->ptr[s->len] = '\0';
	return 0;
}

str2_puts (s, t)
str_t *s;
char *t;
{
	size_t len = strlen (t);

	str2_reserve (s, len);
	memcpy (s->ptr + s->len, t, len);
	s->len += len;
	s->ptr[s->len] = '\0';
	return 0;
}

str2_last (s)
str_t *s;
{
	return s->len > 0 ? s->ptr[s->len - 1] : EOF;
}

str2_pop (s)
str_t *s;
{
	return s->len > 0 ? s->ptr[--s->len] : EOF;
}

str2_chomp (s)
str_t *s;
{
	while (str2_last (s) == '\n')
		str2_pop (s);
	return 0;
}

str2_trim (s)
str_t *s;
{
	size_t i;

	while (isspace (str2_last (s)))
		str2_pop (s);

	for (i = 0; i < s->len && isspace (s->ptr[i]); ++i);

	memmove (s->ptr, s->ptr + i, s->len - i);
	s->len -= i;
	s->ptr[s->len] = '\0';
	return 0;
}

char *
str2_release (s)
str_t *s;
{
	char *t;
	t = realloc (s->ptr, s->len + 1);
	memset (s, 0, sizeof (*s));
	return t;
}

str2_reset (s)
str_t *s;
{
	if (s->cap == 0) {
		str2_new (s);
	} else {
		s->len = 0;
		s->ptr[0] = '\0';
	}
	return 0;
}

