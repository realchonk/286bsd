SUBDIRS = make cc/yacc cc/irc cc/cc1 cc/cpp sys

all:
	for subdir in ${SUBDIRS}; do	\
		echo "=== $$subdir";	\
		${MAKE} -C $$subdir;	\
	done

clean:
	for subdir in ${SUBDIRS}; do		\
		echo "=== $$subdir (clean)";	\
		${MAKE} -C $$subdir clean;	\
	done
