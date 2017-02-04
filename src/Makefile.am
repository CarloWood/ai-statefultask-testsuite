AM_CPPFLAGS = -iquote $(top_srcdir) -iquote $(top_srcdir)/cwds

bin_PROGRAMS = aiengine_test

aiengine_test_SOURCES = \
	aiengine_test.cxx

aiengine_test_CXXFLAGS = @LIBCWD_R_FLAGS@
aiengine_test_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

# --------------- Maintainer's Section

if MAINTAINER_MODE
distclean-local:
	rm -f *.s *.ii

clean-local:
endif

MAINTAINERCLEANFILES = $(srcdir)/Makefile.in
