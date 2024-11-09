SUBDIRS = cc/yacc cc/irc cc/cc1 cc/cpp sys

all:
	for subdir in ${SUBDIRS}; do	\
		${MAKE} -C $$subdir;	\
	done

clean:
	for subdir in ${SUBDIRS}; do		\
		${MAKE} -C $$subdir clean;	\
	done
