bin_PROGRAMS = gl-simple-test

AM_CFLAGS = -Wall -O3
AM_LDADD = -lm

gl_simple_test_SOURCES = \
	main.c

gl_simple_test_CFLAGS = \
	$(AM_CFLAGS) \
	@X11_CFLAGS@ \
	@GLES_CFLAGS@

gl_simple_test_LDADD = \
	$(AM_LDADD) \
	@X11_LIBS@ \
	@GLES_LIBS@
