#ifndef FILE_MAKE_H
#define FILE_MAKE_H
#include <sys/time.h>

enum path_type {
	PATH_NULL,
	PATH_SUPER,
	PATH_NAME,
};
struct path {
	enum path_type type;
	char *name;
};

enum scope_type {
	SC_DIR,
	SC_GNU,
};
struct scope {
	struct scope *next;
	enum scope_type type;
	char *name; /* optional */
	struct scope *parent; /* optional */
	char *makefile; /* required */
	union {
		struct directory *dir; /* optional */
		struct gnu *gnu; /* required */
	};
};

struct directory {
	struct scope *subdirs;
	struct file *files;
	struct macro *macros;
	struct macro *emacros;	/* exported macros */
	struct inference *infs;
};

struct gnu {
	char *prog;	/* optional (default: "make") */
};

struct dep {
	struct dep *next;
	struct path *path;
};

struct file {
	struct file *next;
	char *name;
	struct rule *rule; /* optional */
	struct dep *deps, *dtail;
	struct timespec mtime;
};

struct inference {
	struct inference *next;
	char *from, *to;
	struct rule *rule;
	struct dep *deps, *dtail;
};

struct rule {
	char **code;
};

struct macro {
	struct macro *next, *enext;
	char *name;
	char *value;
	int lazy;
};

#endif /* FILE_MAKE_H */
