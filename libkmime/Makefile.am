INCLUDES = $(all_includes)

noinst_LTLIBRARIES = libkmime.la
libkmime_la_LDFLAGS = $(all_libraries) $(KDE_RPATH)
libkmime_la_LIBADD  = $(LIB_KDECORE)
libkmime_la_SOURCES = kmime_codec_base64.cpp kmime_codec_uuencode.cpp \
	kmime_header_parsing.cpp kmime_codec_qp.cpp kmime_codecs.cpp \
	kmime_util.cpp
noinst_HEADERS = kmime_codec_base64.h kmime_codec_uuencode.h \
	kmime_header_parsing.h kmime_codec_qp.h kmime_codecs.h \
	kmime_util.h kmime_warning.h
