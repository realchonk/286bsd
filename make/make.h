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
	char *name; // optional
	struct scope *parent; // optional
	union {
		struct directory *dir; // optional
		struct gnu *gnu; // required
	};
};

struct directory {
	struct scope *subdirs;
	struct file *files;
	struct macro *macros;
	struct macro *emacros;	// exported macros
};

struct gnu {
	char *prog;	// optional (default: "make")
	char *file;	// optional
};

struct file {
	struct file *next;
	char *name;
	struct rule *rule; // optional
	time_t mtime;
};

struct rule {
	struct path **deps;
	char **code;
};

struct macro {
	struct macro *next, *enext;
	char *name;
	char *value;
	int lazy;
};

#endif // FILE_MAKE_H
