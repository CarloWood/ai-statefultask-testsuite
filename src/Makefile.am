AM_CPPFLAGS = -iquote $(top_srcdir) -iquote $(top_srcdir)/cwds

bin_PROGRAMS = helloworld fibonacci fiboquick

helloworld_SOURCES = \
	helloworld.cxx

fibonacci_SOURCES = \
	fibonacci.cxx

fiboquick_SOURCES = \
	fiboquick.cxx

helloworld_CXXFLAGS = @LIBCWD_R_FLAGS@
helloworld_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

fibonacci_CXXFLAGS = @LIBCWD_R_FLAGS@
fibonacci_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

fiboquick_CXXFLAGS =
fiboquick_LDADD =

# --------------- Maintainer's Section

if MAINTAINER_MODE
distclean-local:
	rm -f *.s *.ii

clean-local:
endif

MAINTAINERCLEANFILES = $(srcdir)/Makefile.in
