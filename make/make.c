#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <err.h>
#include "make.h"

#define new(T) ((T *)calloc (1, sizeof (T)))
#define MAKEFILE "MyMakefile"
#define SHELL "sh"

// #define MAKE_DEBUG 1

static struct macro m_shell = {
	.next = NULL,
	.enext = NULL,
	.name = "SHELL",
	.value = SHELL,
	.lazy = 0,
}, m_make = {
	.next = &m_shell,
	.enext = &m_shell,
	.name = "MAKE",
	.value = NULL,
	.lazy = 0,
};

static struct macro *globals = &m_make;

// MISC

char *
ltrim (s)
char *s;
{
	for (; isspace (*s); ++s);
	return s;
}

char *
rtrim (s)
char *s;
{
	size_t i, len;

	len = strlen (s);
	for (i = len; i > 0 && isspace (s[i - 1]); --i)
		s[i - 1] = '\0';

	return s;
}

char *
trim (s)
char *s;
{
	return ltrim (rtrim (s));
}

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

struct timespec
get_mtime (dir, name)
char *dir, *name;
{
	struct stat st;
	char *path = NULL;
	struct timespec t;

	asprintf (&path, "%s/%s", dir, name);

	if (stat (path, &st) == 0) {
		t = st.st_mtim;
	} else {
		memset (&t, 0, sizeof (t));
	}

	free (path);
	return t;
}

struct timespec
now ()
{
	struct timespec t;
	clock_gettime (CLOCK_REALTIME, &t);
	return t;
}

tv_cmp (a, b)
struct timespec *a, *b;
{
	if (a->tv_sec < b->tv_sec) {
		return -1;
	} else if (a->tv_sec > b->tv_sec) {
		return 1;
	} else if (a->tv_nsec < b->tv_nsec) {
		return -1;
	} else if (a->tv_nsec > b->tv_nsec) {
		return 1;
	} else {
		return 0;
	}
}

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

str_chomp ()
{
	while (strbuf_len > 0 && strbuf[strbuf_len - 1] == '\n')
		--strbuf_len;
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

// MACORS

struct macro *
find_emacro (sc, name)
struct scope *sc;
char *name;
{
	struct macro *m;

	if (sc == NULL)
		return NULL;

	for (m = sc->dir->emacros; m != NULL; m = m->enext) {
		if (strcmp (m->name, name) == 0)
			return m;
	}
	
	return find_emacro (sc->parent, name);
}

struct macro *
find_macro (sc, name)
struct scope *sc;
char *name;
{
	struct macro *m;

	for (m = sc->dir->macros; m != NULL; m = m->next) {
		if (strcmp (m->name, name) == 0)
			return m;
	}

	m = find_emacro (sc->parent, name);
	if (m != NULL)
		return m;

	for (m = globals; m != NULL; m = m->next) {
		if (strcmp (m->name, name) == 0)
			return m;
	}

	return NULL;
}

expand_into (sc, s, target)
struct scope *sc;
char *s, *target;
{
	extern expand_macro ();
	struct macro *m;
	char *t;
	int ch;

	while (*s != '\0') {
		if (*s != '$') {
			str_push (*s++);
			continue;
		}
		ch = *++s;
		switch (ch) {
		case '$':
			str_push ('$');
			++s;
			break;
		case '@':
			if (target == NULL)
				errx (1, "cannot use $@ here");
			str_push_str (target);
			++s;
			break;
		case '{':
			// ${name}
			++s;
			t = strchr (s, '}');
			if (t == NULL)
				errx (1, "syntax error: invalid macro: %s", s - 2);
			*t = '\0';
			m = find_macro (sc, s);
			if (m != NULL)
				expand_macro (sc, m);
			*t = '}';
			s = t + 1;
			break;
		default:
			errx (1, "syntax error: invalid escape sequence: %s", s - 2);
		}
	}
	return 0;
}

expand_macro (sc, m)
struct scope *sc;
struct macro *m;
{
	if (m->lazy) {
		expand_into (sc, m->value, NULL);
	} else {
		str_push_str (m->value);
	}
	return 0;
}

char *
expand (sc, s, target)
struct scope *sc;
char *s, *target;
{
	str_reset ();
	expand_into (sc, s, target);
	return str_copy ();
}

char *
evalcom (sc, dir, cmd)
struct scope *sc;
struct path *dir;
char *cmd;
{
	char *args[] = {
		m_shell.value,
		"-c",
		expand (sc, cmd, NULL),
		NULL,
	};
	ssize_t i, n;
	pid_t pid;
	int pipefd[2];
	char buf[64 + 1];

	if (pipe (pipefd) != 0)
		err (1, "pipe()");

	pid = fork ();
	if (pid == -1)
		err (1, "fork()");

	if (pid == 0) {
		close (STDOUT_FILENO);
		close (pipefd[0]);
		if (dup (pipefd[1]) != STDOUT_FILENO)
			err (1, "failed to dup");
		close (STDIN_FILENO);
		if (open ("/dev/null", O_RDONLY) != STDIN_FILENO)
			err (1, "failed to open /dev/null");
		close (pipefd[1]);

		if (chdir (path_to_str (dir)) != 0)
			err (1, "failed to chdir");

		execvp (m_shell.value, args);
		err (1, "failed to launch shell");
	} else {
		close (pipefd[1]);

		str_reset ();

		while ((n = read (pipefd[0], buf, sizeof (buf) - 1)) > 0) {
			for (i = 0; i < n; ++i)
				str_push (buf[i]);
		}
		close (pipefd[0]);

		if (wait (NULL) != pid)
			err (1, "wait()");

	}

	str_chomp ();

	return str_copy ();
}

// PARSER

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

// .include yacc
// .include yacc, DIR
// .include libx, GNU
parse_include (sc, dir, s)
struct scope *sc;
struct path *dir;
char *s;
{
	struct scope *sub;
	char *p, *path;

	sub = new (struct scope);
	sub->next = sc->dir->subdirs;
	sub->parent = sc;
	sc->dir->subdirs = sub;

	p = strtok (s, ",");
	if (p == NULL)
		return -1;

	sub->name = strdup (trim (p));

	asprintf (&path, "%s/%s", path_to_str (dir), sub->name);
	if (access (path, F_OK) != 0)
		errx (1, "%s: directory not found: %s", path_to_str (dir), sub->name);
	free (path);
	
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

struct rule *
parse_rule (sc, dir, s, t)
struct scope *sc;
struct path *dir;
char *s, *t;
{
	struct rule *r;
	struct file *f;
	struct dep *dep;
	char *u, *p;

	r = new (struct rule);
	r->deps = NULL;

	*t = '\0';

	// parse deps
	u = expand (sc, t + 1, NULL);
	for (p = strtok (u, " \t"); p != NULL; p = strtok (NULL, " \t")) {
		dep = new (struct dep);
		dep->next = r->deps;
		dep->path = parse_path (p);
		r->deps = dep;
	}	
	free (u);

	u = expand (sc, s, NULL);
	for (p = strtok (u, " \t"); p != NULL; p = strtok (NULL, " \t")) {
		// TODO: check name, handle duplicates
		f = new (struct file);
		f->next = sc->dir->files;
		f->name = strdup (p);
		f->rule = r;
		f->mtime = get_mtime (path_to_str (dir), f->name);
		sc->dir->files = f;
	}
	free (u);
	return r;
}

parse (sc, dir, path) 
struct scope *sc;
struct path *dir;
char *path;
{
	struct macro *m;
	struct rule *r = NULL;
	size_t len, cap;
	char *s, *t;
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
		} else if (starts_with (s, ".export ")) {
			t = trim (s + 8);

			for (m = sc->dir->emacros; m != NULL; m = m->enext) {
				if (strcmp (m->name, t) == 0)
					goto ok; // already exported
			}

			for (m = sc->dir->macros; m != NULL; m = m->next) {
				if (strcmp (m->name, t) == 0) {
					m->enext = sc->dir->emacros;
					sc->dir->emacros = m;
					goto ok;
				}
			}

			errx (1, "%s:%d: no such macro: %s", path, ln, t);
		ok:	;
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
			// TODO: check name
			*t = '\0';

			m = new (struct macro);
			m->next = sc->dir->macros;

			if (t[-1] == '!') {
				t[-1] = '\0';
				m->lazy = 0;
				m->value = evalcom (sc, dir, trim (t + 1));
			} else {
				m->lazy = 1;
				m->value = strdup (trim (t + 1));
			}
			m->name = strdup (trim (s));
			sc->dir->macros = m;
		} else if ((t = strchr (s, ':')) != NULL) {
			r = parse_rule (sc, dir, s, t);

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

parse_dir (sc, dir)
struct scope *sc;
struct path *dir;
{
	char *path;

	asprintf (&path, "%s/" MAKEFILE, path_to_str (dir));
	parse (sc, dir, path);
	free (path);

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

runcom (sc, prefix, cmd, target)
struct scope *sc;
struct path *prefix;
char *cmd, *target;
{
	char *line;
	int ec, q = 0;

	if (*cmd == '@') {
		q = 1;
		++cmd;
	}

	cmd = expand (sc, cmd, target);
	
	if (!q)
		printf ("[%s] $ %s\n", path_to_str (prefix), cmd);

	asprintf (&line, "cd '%s' && %s", path_to_str (prefix), cmd);
	ec = system (line);

	free (line);
	free (cmd);

	return ec;
}

rungnu (sc, prefix, gnu, rule, q)
struct scope *sc;
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

	ec = runcom (sc, prefix, cmd, NULL);
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
	struct timespec t;
	struct file *f;

	// TODO: template rules

	t = get_mtime (dir, name);
	if (t.tv_sec == 0)
		return NULL;

	f = new (struct file);
	f->next = sc->dir->files;
	f->name = strdup (name);
	f->rule = NULL;
	f->mtime = t;
	sc->dir->files = f;

	return f;
}

struct timespec
build_file (sc, name, prefix)
struct scope *sc;
char *name;
struct path *prefix;
{
	extern struct timespec build_dir ();
	int needs_update;
	struct scope *sub;
	struct path *new_prefix = NULL;
	struct file *f;
	struct timespec t, maxt;
	struct dep *dep;
	char **s;

#if MAKE_DEBUG
	printf ("dir %s", path_to_str (prefix));
	if (name)
		printf (" (%s)", name);
	printf (" ...\n");
#endif

	switch (sc->type) {
	case SC_DIR:
		if (sc->dir == NULL)
			parse_dir (sc, prefix);

		if (name != NULL) {
			f = find_file (sc->dir, name);
			if (f == NULL) {
				for (sub = sc->dir->subdirs; sub != NULL; sub = sub->next) {
					if (strcmp (sub->name, name) == 0) {
						tmppath.name = name;
						new_prefix = path_cat (prefix, &tmppath);
						t = build_file (sub, NULL, new_prefix);
						goto ret;
					}
				}
				// TODO: templates
				f = try_find (sc, path_to_str (prefix), name);
				if (f == NULL)
					errx (1, "%s: no such file: %s", path_to_str (prefix), name);
			} else {
				f->mtime = get_mtime (path_to_str (prefix), name);
			}
		} else {
			f = sc->dir->files;
			if (f == NULL)
				errx (1, "%s: nothing to build", path_to_str (prefix));
			for (; f->next != NULL; f = f->next);
		}

		if (f->rule == NULL) {
			if (f->mtime.tv_sec > 0) {
				t = f->mtime;
				goto ret;
			} else {
				errx (1, "%s: no rule to build: %s", path_to_str (prefix), name);
			}
		}

		needs_update = (f->mtime.tv_sec == 0);
		maxt = f->mtime;

		for (dep = f->rule->deps; dep != NULL; dep = dep->next) {
			t = build_dir (sc, dep->path, prefix);
			if (tv_cmp (&t, &f->mtime) >= 0)
				needs_update = 1;
			if (tv_cmp (&t, &maxt) > 0)
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

		for (; *s != NULL; ++s) {
			if (runcom (sc, prefix, *s, f->name) != 0)
				errx (1, "command failed: %s", *s);
		}

		t = f->mtime = now ();
		goto ret;
	case SC_GNU:
		// first check if the target is already built
		if (rungnu (sc, prefix, sc->gnu, name, 1) == 0) {
			t.tv_sec = 0;
			goto ret;
		}

		if (rungnu (sc, prefix, sc->gnu, name, 0) != 0)
			errx (1, "building foreign directory failed");

		t = now ();
		goto ret;
	}
	
ret:
	if (new_prefix != NULL)
		free (new_prefix);
	return t;
}

struct timespec
build_dir (sc, path, prefix)
struct scope *sc;
struct path *path, *prefix;
{
	struct path *new_prefix = NULL;
	struct scope *sub;
	struct timespec t;

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
				parse_dir (sc, prefix);

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

struct timespec
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
	struct macro *m;
	struct dep *dep;
	struct file *f;
	struct rule *r;
	char **s;

	if (sc->type != SC_DIR || sc->dir == NULL)
		errx (1, "print_sc(): must be of type SC_DIR");

	for (m = sc->dir->macros; m != NULL; m = m->next) {
		printf ("%s = %s\n", m->name, m->value);
	}

	printf ("\n");

	for (f = sc->dir->files; f != NULL; f = f->next) {
		printf ("%s:", f->name);
	
		r = f->rule;
		if (r != NULL) {
			for (dep = r->deps; dep != NULL; dep = dep->next)
				printf (" %s", path_to_str (dep->path));

			printf ("\n");

			for (s = r->code; *s != NULL; ++s)
				printf ("\t%s\n", *s);
		}
		printf ("\n");
	}
	
	for (sub = sc->dir->subdirs; sub != NULL; sub = sub->next) {
		switch (sub->type) {
		case SC_DIR:
			printf (".include %s, DIR\n", sub->name);
			break;
		case SC_GNU:
			printf (".include %s, GNU", sub->name);
			if (sub->gnu->prog)
				printf (", %s", sub->gnu->prog);
			if (sub->gnu->file)
				printf (", %s", sub->gnu->file);
			printf ("\n");
			break;
		}
	}

	return 0;
}

usage ()
{
	fprintf (stderr, "usage: %s [-p] [target...]\n", m_make.value);
	return 1;
}

main (argc, argv)
char **argv;
{
	struct scope *sc;
	struct path *path;
	int i, option, pr = 0;

	m_make.value = argv[0];

	while ((option = getopt (argc, argv, "p")) != -1) {
		switch (option) {
		case 'p':
			pr = 1;
			break;
		case '?':
			return usage ();
		default:
			errx (1, "unexpected option: -%c", option);
		}
	}

	argv += optind;
	argc -= optind;

	sc = parse_recursive (".");

	if (pr) {
		print_sc (sc);
		return 0;
	}
	
	if (argc == 0) {
		build (sc, &path_null);
	} else {
		for (i = 0; i < argc; ++i) {
			path = parse_path (argv[i]);
			build (sc, path);
		}
	}


	return 0;
}


