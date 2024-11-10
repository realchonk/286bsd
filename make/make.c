#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <err.h>
#include "make.h"

// EXAMPLE

extern struct scope sc_top, sc_cc, sc_cc1, sc_yacc, sc_make, sc_libx;
extern struct directory dir_top, dir_cc, dir_cc1, dir_yacc, dir_make;
extern struct file f_all, f_cc1, f_cc1_c, f_yacc, f_yacc_c, f_make, f_make_c;
extern struct rule r_all, r_cc1, r_yacc, r_make;
extern struct gnu gnu_libx;

struct scope sc_top = {
	.next = NULL,
	.type = SC_DIR,
	.name = NULL,
	.parent = NULL,
	.dir = &dir_top,
}, sc_cc = {
	.next = &sc_make,
	.type = SC_DIR,
	.name = "cc",
	.parent = &sc_top,
	.dir = &dir_cc,
}, sc_cc1 = {
	.next = &sc_yacc,
	.type = SC_DIR,
	.name = "cc1",
	.parent = &sc_cc,
	.dir = &dir_cc1,
}, sc_yacc = {
	.next = NULL,
	.type = SC_DIR,
	.name = "yacc",
	.parent = &sc_cc,
	.dir = &dir_yacc,
}, sc_make = {
	.next = &sc_libx,
	.type = SC_DIR,
	.name = "make",
	.parent = &sc_top,
	.dir = &dir_make,
}, sc_libx = {
	.next = NULL,
	.type = SC_GNU,
	.name = "libx",
	.parent = &sc_top,
	.gnu = &gnu_libx,
};

struct directory dir_top = {
	.subdirs = &sc_cc,
	.files = &f_all,
}, dir_cc = {
	.subdirs = &sc_cc1,
	.files = NULL,
}, dir_cc1 = {
	.subdirs = NULL,
	.files = &f_cc1,
}, dir_yacc = {
	.subdirs = NULL,
	.files = &f_yacc,
}, dir_make = {
	.subdirs = NULL,
	.files = &f_make,
};

struct gnu gnu_libx = {
	.prog = NULL,
	.file = NULL,
};

struct file f_all = {
	.next = NULL,
	.name = "all",
	.rule = &r_all,
	.mtime = 0,
}, f_cc1 = {
	.next = &f_cc1_c,
	.name = "cc1",
	.rule = &r_cc1,
	.mtime = 0,
}, f_cc1_c = {
	.next = NULL,
	.name = "cc1.c",
	.rule = NULL,
	.mtime = 1,
}, f_yacc = {
	.next = &f_yacc_c,
	.name = "yacc",
	.rule = &r_yacc,
	.mtime = 0,
}, f_yacc_c = {
	.next = NULL,
	.name = "yacc.c",
	.rule = NULL,
	.mtime = 1,
}, f_make = {
	.next = &f_make_c,
	.name = "make",
	.rule = &r_make,
	.mtime = 0,
}, f_make_c = {
	.next = NULL,
	.name = "make.c",
	.rule = NULL,
	.mtime = 1,
};

// all
struct path dep_all_make[] = {
	{ .type = PATH_NAME, .name = "make" },
	{ .type = PATH_NULL, .name = NULL },
};

struct path dep_all_cc_cc1[] = {
	{ .type = PATH_NAME, .name = "cc" },
	{ .type = PATH_NAME, .name = "cc1" },
	{ .type = PATH_NULL, .name = NULL },
};

struct path *deps_all[] = {
	dep_all_make,
	dep_all_cc_cc1,
	NULL
};

char *code_all[] = {
	"echo DONE",
	NULL
};

// cc/cc1/cc1
struct path dep_cc1_yacc[] = {
	{ .type = PATH_SUPER, .name = NULL },
	{ .type = PATH_NAME,  .name = "yacc" },
	{ .type = PATH_NULL,  .name = NULL },
};

struct path dep_cc1_cc1_c[] = {
	{ .type = PATH_NAME,   .name = "cc1.c" },
	{ .type = PATH_NULL,   .name = NULL },
};

struct path *deps_cc1[] = {
	dep_cc1_yacc,
	dep_cc1_cc1_c,
	NULL
};

char *code_cc1[] = {
	"cc -o cc1 cc1.c",
	NULL,
};

// cc/yacc/yacc
struct path dep_yacc_yacc_c[] = {
	{ .type = PATH_NAME, .name = "yacc.c" },
	{ .type = PATH_NULL, .name = NULL },
};

struct path dep_yacc_libx[] = {
	{ .type = PATH_SUPER, .name = NULL },
	{ .type = PATH_SUPER, .name = NULL },
	{ .type = PATH_NAME, .name = "libx" },
	{ .type = PATH_NULL, .name = NULL },
};

struct path *deps_yacc[] = {
	dep_yacc_yacc_c,
	dep_yacc_libx,
	NULL
};

char *code_yacc[] = {
	"cc -o yacc yacc.c",
	NULL
};

// make/make
struct path dep_make_make_c[] = {
	{ .type = PATH_NAME, .name = "make.c" },
	{ .type = PATH_NULL, .name = NULL },
};

struct path *deps_make[] = {
	dep_make_make_c,
	NULL
};

char *code_make[] = {
	"cc -o make make.c",
	NULL
};

struct rule r_all = {
	.deps = deps_all,
	.code = code_all,
}, r_cc1 = {
	.deps = deps_cc1,
	.code = code_cc1,
}, r_yacc = {
	.deps = deps_yacc,
	.code = code_yacc,
}, r_make = {
	.deps = deps_make,
	.code = code_make,
};

// PATH LOGIC

static struct path path_null = { .type = PATH_NULL, .name = NULL };
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
		p[len].type = PATH_NAME;
		p[len].name = comp->name;
		break;
	}

	return p;
}

char *
path_to_str (p)
struct path *p;
{
	static char *buf = NULL;
	static size_t buf_len = 0;
	size_t i, j, len = 2;

	for (i = 0; p[i].type != PATH_NULL; ++i) {
		switch (p[i].type) {
		case PATH_NULL:
			abort ();
		case PATH_SUPER:
			len += 3;
			break;
		case PATH_NAME:
			len += strlen (p[i].name) + 1;
			break;
		}
	}

	if (len > buf_len) {
		buf_len = len;
		buf = realloc (buf, buf_len);
	}

	j = 1;
	buf[0] = '.';
	buf[1] = '\0';

	for (i = 0; p[i].type != PATH_NULL; ++i) {
		switch (p[i].type) {
		case PATH_NULL:
			abort ();
		case PATH_SUPER:
			j += snprintf (buf + j, len - j, "/..");
			break;
		case PATH_NAME:
			j += snprintf (buf + j, len - j, "/%s", p[i].name);
			break;
		}
	}

	return buf;
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
						tmppath.name = name;
						new_prefix = path_cat (prefix, &tmppath);
						t = build_file (sub, NULL, new_prefix);
						goto ret;
					}
				}
				errx (1, "no such file: %s", name);
			}
		} else {
			f = sc->dir->files;
			if (f == NULL)
				errx (1, "nothing to build");
		}

		if (f->rule == NULL) {
			if (f->mtime > 0) {
				t = f->mtime;
				goto ret;
			} else {
				errx (1, "no rule to build: %s", name);
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
				errx (1, "invalid path");

			if (sc->dir == NULL)
				errx (1, "TODO: read subdir: %s", sc->name);

			new_prefix = path_cat (prefix, &path[0]);

			for (sub = sc->dir->subdirs; sub != NULL; sub = sub->next) {
				if (strcmp (sub->name, path[0].name) == 0) {
					t = build_dir (sub, path + 1, new_prefix);
					goto ret;
				}
			}
			
			errx (1, "invalid subdir: %s", path[0].name);
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

main ()
{
	struct path path[] = {
		{ .type = PATH_NAME, .name = "all" },
		{ .type = PATH_NULL, .name = NULL },
	};

	build (&sc_top, path);
	return 0;
}


