INCLUDES= -I$(top_srcdir) -I$(top_srcdir)/kalarm $(all_includes)

noinst_LTLIBRARIES = libkalarmd.la

libkalarmd_la_LDFLAGS = $(all_libraries) $(KDE_RPATH)
libkalarmd_la_LIBADD  = $(LIB_KDECORE)
libkalarmd_la_SOURCES =  alarmdaemoniface.stub alarmguiiface.stub \
	adcalendarbase.cpp

bin_PROGRAMS = kalarmd

kalarmd_LDFLAGS = $(all_libraries) $(KDE_RPATH)
kalarmd_LDADD = $(LIB_KDEUI) $(top_builddir)/libkcal/libkcal.la libkalarmd.la
kalarmd_SOURCES = alarmdaemon.cpp main.cpp alarmapp.cpp \
        adcalendar.cpp adconfigdatabase.cpp clientinfo.cpp \
        adconfigdatarw.cpp \
        alarmdaemoniface.skel
kalarmd_COMPILE_FIRST = alarmguiiface_stub.h

noinst_HEADERS = alarmapp.h alarmdaemon.h alarmdaemoniface.h \
	alarmguiiface.h adcalendarbase.h adcalendar.h \
	adconfigdatabase.h adconfigdatarw.h clientinfo.h

METASOURCES = AUTO
 
autostart_DATA = kalarmd.autostart.desktop
autostartdir = $(datadir)/autostart

app_DATA = kalarmd.desktop
appdir = $(kde_appsdir)/.hidden

include ../../admin/Doxyfile.am
