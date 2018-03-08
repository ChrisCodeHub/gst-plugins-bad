
plugin_LTLIBRARIES = libgstleanTsChecker.la

libgstleanTsChecker_la_SOURCES = gstleanTsChecker.c
libgstleanTsChecker_la_CFLAGS = \
  $(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS)
libgstleanTsChecker_la_LIBADD =  \
	$(GST_PLUGINS_BASE_LIBS) -lgstvideo-@GST_API_VERSION@ \
	$(GST_BASE_LIBS) \
	$(GST_LIBS)
libgstleanTsChecker_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

noinst_HEADERS = gstleanTsChecker.h
