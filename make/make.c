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

struct path path_null[] = {
	{ .type = PATH_NULL, .name = NULL },
};

static struct path bpath[128];
static struct path tmppath = { .type = PATH_NAME, .name = NULL };

print_path (p)
struct path *p;
{
	printf (".");
	for (;; ++p) {
		switch (p->type) {
		case PATH_NULL:
			return 0;
		case PATH_SUPER:
			printf ("/..");
			break;
		case PATH_NAME:
			printf ("/%s", p->name);
			break;
		}
	}
}

size_t
bpath_len ()
{
	size_t i;

	for (i = 0; bpath[i].type != PATH_NULL; ++i);

	return i;
}

bpath_restore (len)
size_t len;
{
	bpath[len].type = PATH_NULL;
	return 0;
}

bpath_append (p)
struct path *p;
{
	size_t i;

	i = bpath_len ();

	switch (p->type) {
	case PATH_NULL:
		break;
	case PATH_SUPER:
		if (i == 0 || bpath[i - 1].type == PATH_SUPER) {
			bpath[i] = *p;
			bpath[i + 1].type = PATH_NULL;
		} else {
			bpath[i - 1].type = PATH_NULL;
		}
		break;
	case PATH_NAME:
		bpath[i] = *p;
		bpath[i + 1].type = PATH_NULL;
		break;
	}
	return 0;
}

char *
bpath_to_str ()
{
	size_t i, j, len = 2;
	char *s;

	for (i = 0; bpath[i].type != PATH_NULL; ++i) {
		switch (bpath[i].type) {
		case PATH_NULL:
			abort ();
		case PATH_SUPER:
			len += 3;
			break;
		case PATH_NAME:
			len += strlen (bpath[i].name) + 1;
			break;
		}
	}

	s = malloc (len);
	j = 1;
	s[0] = '.';
	s[1] = '\0';
	for (i = 0; bpath[i].type != PATH_NULL; ++i) {
		switch (bpath[i].type) {
		case PATH_NULL:
			abort ();
		case PATH_SUPER:
			j += snprintf (s + j, len - j, "/..");
			break;
		case PATH_NAME:
			j += snprintf (s + j, len - j, "/%s", bpath[i].name);
			break;
		}
	}
	return s;
}

// RUN COMMANDS

runcom (dir, cmd)
char *dir, *cmd;
{
	printf ("[%s] $ %s\n", dir, cmd);
	return 0;
}

rungnu (dir, gnu, rule, q)
struct gnu *gnu;
char *dir, *rule;
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

	ec = runcom (dir, cmd);
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
build_file (sc, name)
struct scope *sc;
char *name;
{
	extern time_t build ();
	int needs_update;
	struct scope *sub;
	struct path **p;
	struct file *f;
	size_t len;
	time_t t, mt;
	char **s, *dir;

	dir = bpath_to_str ();

	printf ("dir %s", dir);
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
						bpath_append (&tmppath);
						t = build_file (sub, NULL);
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
		len = bpath_len ();
		mt = f->mtime;

		for (p = f->rule->deps; *p != NULL; ++p) {
			t = build (sc, *p);
			if (t >= f->mtime)
				needs_update = 1;
			if (t >= mt)
				mt = t;
			bpath_restore (len);
		}

		if (!needs_update) {
			t = f->mtime;
			goto ret;
		}

		s = f->rule->code;

		// rule is a "sum" rule
		if (s == NULL || *s == NULL) {
			t = mt;
			goto ret;
		}

		// TODO: check exit code
		for (; *s != NULL; ++s)
			runcom (dir, *s);

		t = f->mtime = time (NULL);
		goto ret;
	case SC_GNU:
		if (rungnu (dir, sc->gnu, name, 1) == 0) {
			t = 0;
			goto ret;
		}

		// TODO: check exit code
		rungnu (dir, sc->gnu, name, 0);
		t = time (NULL);
		goto ret;
	}
	
ret:
	free (dir);
	return t;
}

time_t
build (sc, path)
struct scope *sc;
struct path *path;
{
	struct scope *sub;


	switch (path[0].type) {
	case PATH_SUPER:
		bpath_append (&path[0]);
		return build (sc->parent, path + 1);
	case PATH_NULL:
		return build_file (sc, NULL);
	case PATH_NAME:
		if (path[1].type == PATH_NULL) {
			return build_file (sc, path[0].name);
		} else {
			if (sc->type != SC_DIR)
				errx (1, "invalid path");

			if (sc->dir == NULL)
				errx (1, "TODO: read subdir: %s", sc->name);

			bpath_append (&path[0]);

			for (sub = sc->dir->subdirs; sub != NULL; sub = sub->next) {
				if (strcmp (sub->name, path[0].name) == 0)
					return build (sub, path + 1);
			}
			
			errx (1, "invalid subdir: %s", path[0].name);
		}
	}
}

// MAIN

main ()
{
	struct path path[] = {
		{ .type = PATH_NAME, .name = "all" },
		//{ .type = PATH_SUPER, .name = NULL },
		//{ .type = PATH_NAME, .name = "cc" },
		//{ .type = PATH_NAME, .name = "cc1" },
		//{ .type = PATH_NAME, .name = "cc1" },
		{ .type = PATH_NULL, .name = NULL },
	};

	bpath[0].type = PATH_NULL;

	build (&sc_top, path);
	return 0;
}


