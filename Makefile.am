# set the include path for X, qt and KDE and calendar library
INCLUDES= -I$(top_srcdir) $(all_includes)

bin_PROGRAMS = kalarm
kalarm_SOURCES = main.cpp kalarmapp.cpp editdlg.cpp mainwindowbase.cpp mainwindow.cpp \
	messagewin.cpp msgevent.cpp alarmcalendar.cpp \
	prefsettings.cpp prefs.cpp prefdlg.cpp \
	traywindow.cpp daemongui.cpp alarmguiiface.skel \
	fontcolour.cpp colourcombo.cpp datetime.cpp \
	alarmlistview.cpp spinbox2.cpp
kalarm_LDFLAGS = $(all_libraries) $(KDE_RPATH)
kalarm_LDADD   = $(LIB_KDEUI) $(top_builddir)/libkcal/libkcal.la $(top_builddir)/kalarmd/libkalarmd.la

noinst_HEADERS = kalarm.h kalarmapp.h mainwindow.h messagewin.h editdlg.h \
	mainwindowbase.h traywindow.h daemongui.h alarmguiiface.h \
	msgevent.h alarmcalendar.h alarmlistview.h datetime.h \
	fontcolour.h colourcombo.h \
	spinbox2.h prefs.h prefsettings.h prefdlg.h

METASOURCES = AUTO

SUBDIRS = pixmaps 

alarmguiiface.h: $(top_srcdir)/kalarmd/alarmguiiface.h
	cp $(top_srcdir)/kalarmd/alarmguiiface.h .

autostart_DATA = kalarm.tray.desktop
autostartdir = $(prefix)/share/autostart

kdelnkdir = $(kde_appsdir)/Applications
kdelnk_DATA = kalarm.desktop

KDE_ICON = AUTO

messages: 
	LIST=`find . -name \*.h -o -name \*.hh -o -name \*.H -o -name \*.hxx -o -name \*.hpp -o -name \*.cpp -o -name \*.cc -o -name \*.cxx -o -name \*.ecpp -o -name \*.C`; \
	if test -n "$$LIST"; then \
	 $(XGETTEXT) $$LIST -o $(podir)/kalarm.pot; \
	fi
