#include <stdlib.h>
#include <string.h>

#include "as.h"
#include "nametab.h"

#define RADIX	257

#define NAME_DUMMY 0

static struct name nametab[RADIX];
int name_fill;

static unsigned long
djbhash(name)
	const char *name;
{
	unsigned long hash = 5381;
	int c;

	while (c = (unsigned char)*name++, c != '\0')
		hash = 33 * hash + c;

	return (hash);
}

/*
 * Return an entry for name.
 * If none exists, make a dummy entry (type == 0) and return it.
 */
struct name *
name_lookup(name)
	const char *name;
{
	struct name *entry;
	unsigned long hash;

	hash = djbhash(name);
	for (entry = &nametab[hash % RADIX]; entry->sym != NULL; entry = entry->next) {
		if (*entry->sym == name || !strcmp(*entry->sym, name))
			return (entry);

		if (!entry->next) {
			/* allocate dummy name structure */
			entry->next = malloc(sizeof *entry->next);
			if (!entry->next)
				die("out of memory");

			entry = entry->next;
			entry->next = NULL;
			break;
		}
	}

	/* dummy symbol, to be released later */
	entry->sym = malloc(sizeof *entry->sym);
	if (!entry->sym)
		die("out of memory");
	entry->sym = strdup(name);
	entry->type = NAME_DUMMY;

	return (entry);
}

/* return next symbol of given type or NULL if none */
char **
name_next(entry, type)
	struct name **entry;
{
	const char *name, **sym;

	for (name = *(*entry)->sym; *entry != NULL; *entry = (*entry)->next) {
		if (!((*entry)->type & type))
			continue;

		sym = (*entry)->sym;
		if (name == *sym || !strcmp(name, *sym)) {
			*entry = (*entry)->next;
			return (sym);
		}
	}

	return (NULL);
}

/*
 * Enter sym of type into the symbol table at entry.
 * entry must be the result of name_lookup(*sym).
 */
void
name_put(entry, sym, type)
	struct name *entry;
	const char **sym;
{
	if (entry->type == NAME_DUMMY) {
		free(entry->sym);
		entry->sym = sym;
		entry->type = type;

		return;
	}

	/* no dummy symbol: allocate a new name structure */
	while (entry->next != NULL)
		entry = entry->next;

	entry->next = malloc(sizeof *entry->next);
	if (!entry->next)
		die("out of memory");

	entry = entry->next;
	entry->next = NULL;
	entry->sym = sym;
	entry->type = type;
}
