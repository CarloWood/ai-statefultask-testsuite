AM_CPPFLAGS = -iquote $(top_srcdir) -iquote $(top_srcdir)/cwd

bin_PROGRAMS = aiengine_test

aiengine_test_SOURCES = \
	aiengine_test.cxx

aiengine_test_CXXFLAGS = @LIBCWD_FLAGS@
aiengine_test_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils.la ../cwd/libcwd.la @LIBCWD_LIBS@

# --------------- Maintainer's Section

if MAINTAINER_MODE
distclean-local:
	rm -f *.s *.ii

clean-local:
endif

MAINTAINERCLEANFILES = $(srcdir)/Makefile.in
