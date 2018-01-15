
plugin_LTLIBRARIES = libgstchrisFilter.la

libgstchrisFilter_la_SOURCES = gstchrisFilter.c
libgstchrisFilter_la_CFLAGS = \
  $(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS)
libgstchrisFilter_la_LIBADD =  \
	$(GST_PLUGINS_BASE_LIBS) -lgstvideo-@GST_API_VERSION@ \
	$(GST_BASE_LIBS) \
	$(GST_LIBS)
libgstchrisFilter_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

noinst_HEADERS = gstchrisFilter.h
