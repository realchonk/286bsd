#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <err.h>
#include "make.h"

#define new(T) ((T *)calloc (1, sizeof (T)))
#define MAKEFILE "MyMakefile"

// STRING BUFFER

static char *strbuf = NULL;
static size_t strbuf_len = 0, strbuf_cap = 0;

str_reset ()
{
	strbuf_len = 0;
	return 0;
}

str_push (ch)
{
	if (strbuf_len == strbuf_cap) {
		strbuf_cap = strbuf_cap > 0 ? strbuf_cap * 2 : 10;
		strbuf = realloc (strbuf, strbuf_cap + 1);
	}
	strbuf[strbuf_len++] = ch;
	return 0;
}

str_push_str (s)
char *s;
{
	while (*s != '\0')
		str_push (*s++);
	return 0;
}

char *
str_get ()
{
	if (strbuf == NULL) {
		strbuf_len = 0;
		strbuf_cap = 10;
		strbuf = malloc (strbuf_cap + 1);
	}
	strbuf[strbuf_len] = '\0';
	return strbuf;
}

char *
str_copy ()
{
	return strdup (str_get ());
}

// PATH LOGIC

static struct path path_null = { .type = PATH_NULL, .name = NULL };
static struct path path_super = { .type = PATH_SUPER, .name = NULL };
static struct path tmppath = { .type = PATH_NAME, .name = NULL };

// return the number of path components (excl. PATH_NULL).
size_t
path_len (p)
struct path *p;
{
	size_t i;

	for (i = 0; p[i].type != PATH_NULL; ++i);

	return i;
}

struct path *
path_cpy (old, old_len, new_len)
struct path *old;
size_t old_len, new_len;
{
	struct path *p;

	p = calloc (new_len + 1, sizeof (struct path));
	memcpy (p, old, old_len * sizeof (struct path));
	p[new_len].type = PATH_NULL;

	return p;
}

struct path *
path_cat (old, comp)
struct path *old, *comp;
{
	struct path *p;
	size_t len;

	len = path_len (old);

	switch (comp->type) {
	case PATH_NULL:
		p = path_cpy (old, len, len);
		break;
	case PATH_SUPER:
		if (len > 0 && old[len - 1].type != PATH_SUPER) {
			--len;
			p = path_cpy (old, len, len);
			break;
		}
		// fallthrough
	case PATH_NAME:
		p = path_cpy (old, len, len + 1);
		p[len] = *comp;
		break;
	}

	return p;
}

char *
path_to_str (p)
struct path *p;
{
	size_t i;

	str_reset ();
	str_push ('.');

	for (i = 0; p[i].type != PATH_NULL; ++i) {
		switch (p[i].type) {
		case PATH_NULL:
			abort ();
		case PATH_SUPER:
			str_push_str ("/..");
			break;
		case PATH_NAME:
			str_push ('/');
			str_push_str (p[i].name);
			break;
		}
	}

	return str_get ();
}

char *
path_basename (p)
struct path *p;
{
	char *s, *t;

	s = realpath (path_to_str (p), NULL);
	t = strdup (basename (s));
	free (s);

	return t;
}

struct path *
parse_path (s)
char *s;
{
	size_t len = 0, cap = 4;
	struct path *p;
	char *last;
	char *t;

	p = calloc (cap + 1, sizeof (struct path));

	for (t = strtok_r (s, "/", &last); t != NULL; t = strtok_r (NULL, "/", &last)) {
		if (strcmp (t, ".") == 0)
			continue;

		if (len == cap) {
			cap *= 2;
			p = reallocarray (p, cap + 1, sizeof (struct path));
		}

		if (strcmp (t, "..") == 0) {
			p[len].type = PATH_SUPER;
		} else {
			p[len].type = PATH_NAME;
			p[len].name = strdup (t);
		}
		++len;
	}
	p[len].type = PATH_NULL;

	return p;
}

// PARSER

starts_with (s, prefix)
char *s, *prefix;
{
	size_t len_s, len_p;

	len_s = strlen (s);
	len_p = strlen (prefix);

	if (len_s < len_p)
		return 0;

	return memcmp (s, prefix, len_p) == 0;
}

time_t
get_mtime (dir, name)
char *dir, *name;
{
	struct stat st;
	char *path = NULL;
	time_t t = 0;

	asprintf (&path, "%s/%s", dir, name);
	if (stat (path, &st) == 0)
		t = st.st_mtime;

	free (path);
	return t;
}

char *
readline (file, ln)
FILE *file;
int *ln;
{
	int ch, eof = 1;

	str_reset ();

	while (1) {
		ch = fgetc (file);
		switch (ch) {
		case EOF:
			goto ret;
		case '\n':
			eof = 0;
			++*ln;
			goto ret;
		case '\\':
			ch = fgetc (file);
			if (ch == '\n') {
				++*ln;
			} else {
				str_push ('\\');
				str_push (ch);
			}
			eof = 0;
			break;
		default:
			str_push (ch);
			eof = 0;
			break;
		}
	}

ret:
	return eof ? NULL : str_copy ();
}

char *
trim (s)
char *s;
{
	for (; isspace (*s); ++s);
	return s;
}

// .include yacc
// .include yacc, DIR
// .include libx, GNU
parse_include (sc, dir, s)
struct scope *sc;
struct path *dir;
char *s;
{
	struct scope *sub;
	char *p;

	sub = new (struct scope);
	sub->next = sc->dir->subdirs;
	sub->parent = sc;
	sc->dir->subdirs = sub;

	p = strtok (s, ",");
	if (p == NULL)
		return -1;

	sub->name = strdup (trim (p));
	
	p = strtok (NULL, ",");
	if (p == NULL) {
		sub->type = SC_DIR;
		sub->dir = NULL;
		return 0;
	}

	p = trim (p);
	if (strcmp (p, "DIR") == 0) {
		sub->type = SC_DIR;
		sub->dir = NULL;
		return 0;
	} else if (strcmp (p, "GNU") == 0) {
		sub->type = SC_GNU;
		sub->gnu = new (struct gnu);
		sub->gnu->prog = NULL;
		sub->gnu->file = NULL;

		p = strtok (NULL, ",");
		if (p == NULL)
			return 0;
		sub->gnu->prog = strdup (trim (p));

		p = strtok (NULL, ",");
		if (p == NULL)
			return 0;
		sub->gnu->file = strdup (trim (p));

		return 0;
	} else {
		return -1;
	}

}

parse (sc, dir, path) 
struct scope *sc;
struct path *dir;
char *path;
{
	struct rule *r = NULL;
	struct file *f;
	size_t len, cap;
	char *s, *t, *p;
	FILE *file;
	int ln = 0;

	file = fopen (path, "r");
	if (file == NULL)
		err (1, "open(\"%s\")", path);

	if (sc->dir == NULL) {
		sc->dir = new (struct directory);
		sc->dir->subdirs = NULL;
		sc->dir->files = NULL;
	}

	for (; (s = readline (file, &ln)) != NULL; free (s)) {
		if (s[0] == '#' || *trim (s) == '\0') {
		} else if (starts_with (s, ".include ")) {
			if (parse_include (sc, dir, s + 9) == -1)
				errx (1, "%s:%d: syntax error", path, ln);
		} else if (s[0] == '\t') {
			if (r == NULL)
				errx (1, "%s:%d: syntax error", path, ln);

			if (len == cap) {
				cap *= 2;
				r->code = reallocarray (r->code, cap + 1, sizeof (char *));
			}

			r->code[len++] = strdup (s + 1);
			r->code[len] = NULL;
		} else if ((t = strchr (s, '=')) != NULL) {
			// TODO: macros
		} else if ((t = strchr (s, ':')) != NULL) {
			r = new (struct rule);

			*t = '\0';

			// parse deps
			len = 0;
			cap = 1;
			r->deps = calloc (cap + 1, sizeof (struct path *));
			for (p = strtok (t + 1, " \t"); p != NULL; p = strtok (NULL, " \t")) {
				if (len == cap) {
					cap *= 2;
					r->deps = reallocarray (r->deps, cap + 1, sizeof (struct path *));
				}
				r->deps[len++] = parse_path (p);
			}	
			r->deps[len] = NULL;

			for (p = strtok (s, " \t"); p != NULL; p = strtok (NULL, " \t")) {
				// TODO: check name, handle duplicates
				f = new (struct file);
				f->next = sc->dir->files;
				f->name = strdup (p);
				f->rule = r;
				f->mtime = get_mtime (path_to_str (dir), f->name);
				sc->dir->files = f;
			}

			len = 0;
			cap = 1;
			r->code = calloc (cap + 1, sizeof (char *));
			r->code[0] = NULL;
		} else {
			warnx ("%s:%d: syntax error", path, ln);
		}
	}

	return 0;
}

struct scope *
parse_recursive (path)
char *path;
{
	struct scope *sc, *entry, *parent;
	struct path *dir;

	//dir = parse_path (realpath (path, NULL));
	dir = parse_path (path);

	sc = new (struct scope);
	sc->next = NULL;
	sc->type = SC_DIR;
	sc->name = path_basename (dir);
	sc->parent = NULL;
	entry = sc;

	asprintf (&path, "%s/" MAKEFILE, path_to_str (dir));
	parse (sc, dir, path);
	free (path);

	while (1) {
		dir = path_cat (dir, &path_super);
		asprintf (&path, "%s/" MAKEFILE, path_to_str (dir));
		if (access (path, F_OK) == -1)
			break;

		parent = new (struct scope);
		parent->next = NULL;
		parent->type = SC_DIR;
		parent->name = path_basename (dir);
		parent->parent = NULL;
		parent->dir = new (struct directory);
		parent->dir->subdirs = sc;
		parent->dir->files = NULL;
		sc->parent = parent;
		sc = parent;

		parse (sc, dir, path);
		free (path);
	}

	return entry;
}

// RUN COMMANDS

runcom (prefix, cmd)
struct path *prefix;
char *cmd;
{
	printf ("[%s] $ %s\n", path_to_str (prefix), cmd);
	return 0;
}

rungnu (prefix, gnu, rule, q)
struct path *prefix;
struct gnu *gnu;
char *rule;
{
	size_t len = 0;
	char *cmd, *prog;
	int ec;

	prog = gnu->prog != NULL ? gnu->prog : "make";

	len += strlen (prog);

	if (q)
		len += 3;

	if (gnu->file != NULL)
		len += strlen (gnu->file) + 4;

	if (rule != NULL)
		len += strlen (rule) + 1;

	cmd = malloc (len + 1);
	strcpy (cmd, prog);

	if (q)
		strcat (cmd, " -q");

	if (gnu->file != NULL) {
		strcat (cmd, " -f ");
		strcat (cmd, gnu->file);
	}

	if (rule != NULL) {
		strcat (cmd, " ");
		strcat (cmd, rule);
	}

	ec = runcom (prefix, cmd);
	free (cmd);
	return ec;
}

// BUILD

struct file *
find_file (dir, name)
struct directory *dir;
char *name;
{
	struct file *f;

	for (f = dir->files; f != NULL; f = f->next) {
		if (strcmp (name, f->name) == 0)
			return f;
	}

	return NULL;
}

struct file *
try_find (sc, dir, name)
struct scope *sc;
char *dir, *name;
{
	struct file *f;
	time_t t;

	// TODO: template rules

	t = get_mtime (dir, name);
	if (t == 0)
		return NULL;

	f = new (struct file);
	f->next = sc->dir->files;
	f->name = strdup (name);
	f->rule = NULL;
	f->mtime = t;
	sc->dir->files = f;

	return f;
}

time_t
build_file (sc, name, prefix)
struct scope *sc;
char *name;
struct path *prefix;
{
	extern time_t build_dir ();
	int needs_update;
	struct scope *sub;
	struct path **p, *new_prefix = NULL;
	struct file *f;
	time_t t, maxt;
	char **s;

	printf ("dir %s", path_to_str (prefix));
	if (name)
		printf (" (%s)", name);
	printf (" ...\n");

	switch (sc->type) {
	case SC_DIR:
		if (sc->dir == NULL)
			errx (1, "TODO: read subdir: %s", sc->name);

		if (name != NULL) {
			f = find_file (sc->dir, name);
			if (f == NULL) {
				for (sub = sc->dir->subdirs; sub != NULL; sub = sub->next) {
					if (strcmp (sub->name, name) == 0) {
						goto build;
					}
				}
				// TODO: templates
				f = try_find (sc, path_to_str (prefix), name);
				if (f == NULL)
					errx (1, "%s: no such file: %s", path_to_str (prefix), name);

				sub = sc;
			build:
				tmppath.name = name;
				new_prefix = path_cat (prefix, &tmppath);
				t = build_file (sub, NULL, new_prefix);
				goto ret;
			}
		} else {
			f = sc->dir->files;
			if (f == NULL)
				errx (1, "%s: nothing to build", path_to_str (prefix));
			for (; f->next != NULL; f = f->next);
		}

		if (f->rule == NULL) {
			if (f->mtime > 0) {
				t = f->mtime;
				goto ret;
			} else {
				errx (1, "%s: no rule to build: %s", path_to_str (prefix), name);
			}
		}

		needs_update = (f->mtime == 0);
		maxt = f->mtime;

		for (p = f->rule->deps; *p != NULL; ++p) {
			t = build_dir (sc, *p, prefix);
			if (t >= f->mtime)
				needs_update = 1;
			if (t > maxt)
				maxt = t;
		}

		if (!needs_update) {
			t = f->mtime;
			goto ret;
		}

		s = f->rule->code;

		// rule is a "sum" rule
		if (s == NULL || *s == NULL) {
			t = maxt;
			goto ret;
		}

		// TODO: check exit code
		for (; *s != NULL; ++s)
			runcom (prefix, *s);

		t = f->mtime = time (NULL);
		goto ret;
	case SC_GNU:
		// first check if the target is already built
		if (rungnu (prefix, sc->gnu, name, 1) == 0) {
			t = 0;
			goto ret;
		}

		// TODO: check exit code
		rungnu (prefix, sc->gnu, name, 0);
		t = time (NULL);
		goto ret;
	}
	
ret:
	if (new_prefix != NULL)
		free (new_prefix);
	return t;
}

time_t
build_dir (sc, path, prefix)
struct scope *sc;
struct path *path, *prefix;
{
	struct path *new_prefix = NULL;
	struct scope *sub;
	time_t t;

	switch (path[0].type) {
	case PATH_SUPER:
		new_prefix = path_cat (prefix, &path[0]);
		t = build_dir (sc->parent, path + 1, new_prefix);
		goto ret;
	case PATH_NULL:
		return build_file (sc, NULL, prefix);
	case PATH_NAME:
		if (path[1].type == PATH_NULL) {
			return build_file (sc, path[0].name, prefix);
		} else {
			if (sc->type != SC_DIR)
				errx (1, "%s: invalid path", path_to_str (prefix));

			if (sc->dir == NULL)
				errx (1, "TODO: read subdir: %s", sc->name);

			new_prefix = path_cat (prefix, &path[0]);

			for (sub = sc->dir->subdirs; sub != NULL; sub = sub->next) {
				if (strcmp (sub->name, path[0].name) == 0) {
					t = build_dir (sub, path + 1, new_prefix);
					goto ret;
				}
			}
			
			errx (1, "%s: invalid subdir: %s", path_to_str (prefix), path[0].name);
		}
	}

ret:
	if (new_prefix)
		free (new_prefix);
	return t;
}

time_t
build (sc, path)
struct scope *sc;
struct path *path;
{
	return build_dir (sc, path, &path_null);
}

// MAIN

print_sc (sc)
struct scope *sc;
{
	struct scope *sub;
	struct path **p;
	struct file *f;
	struct rule *r;
	char **s;

	if (sc->name != NULL)
		printf ("=== %s\n", sc->name);

	switch (sc->type) {
	case SC_DIR:
		if (sc->dir == NULL) {
			printf ("(unread)\n");
			break;
		}
		for (sub = sc->dir->subdirs; sub != NULL; sub = sub->next) {
			printf (".scinclude %s\n", sub->name);
		}
		for (f = sc->dir->files; f != NULL; f = f->next) {
			printf ("%s:", f->name);
			r = f->rule;
			if (r != NULL) {
				for (p = r->deps; *p != NULL; ++p)
					printf (" %s", path_to_str (*p));
				
				printf ("\n");

				for (s = r->code; *s != NULL; ++s)
					printf ("\t%s\n", *s);
			}
			printf ("\n");
		}
		break;
	case SC_GNU:
		printf ("prog: %s\n", sc->gnu->prog);
		printf ("file: %s\n", sc->gnu->file);
		break;
	}
	return 0;
}

main ()
{
	struct scope *sc;
	struct path *path = parse_path ("all");

	sc = parse_recursive (".");
	print_sc (sc);

	build (sc, path);

	return 0;
}


