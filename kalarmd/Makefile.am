INCLUDES= -I$(top_srcdir) -I$(top_srcdir)/kalarm $(all_includes)

noinst_LTLIBRARIES = libkalarmd.la

libkalarmd_la_LDFLAGS = $(all_libraries) $(KDE_RPATH)
libkalarmd_la_LIBADD  = $(LIB_KDECORE)
libkalarmd_la_SOURCES =  alarmdaemoniface.stub alarmguiiface.stub

bin_PROGRAMS = kalarmd

kalarmd_LDFLAGS = $(all_libraries) $(KDE_RPATH)
kalarmd_LDADD = $(LIB_KDEUI) $(top_builddir)/libkcal/libkcal.la libkalarmd.la
kalarmd_SOURCES = alarmdaemon.cpp admain.cpp adapp.cpp \
        adcalendar.cpp adconfigdata.cpp clientinfo.cpp \
        alarmdaemoniface.skel
kalarmd_COMPILE_FIRST = alarmguiiface_stub.h

noinst_HEADERS = adapp.h alarmdaemon.h alarmdaemoniface.h \
	alarmguiiface.h adcalendar.h adconfigdata.h clientinfo.h

METASOURCES = AUTO
 
autostart_DATA = kalarmd.autostart.desktop
autostartdir = $(datadir)/autostart

app_DATA = kalarmd.desktop
appdir = $(kde_appsdir)/.hidden

include $(top_srcdir)/doc/api/Doxyfile.am
