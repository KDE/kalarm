SUBDIRS = widgets pixmaps

INCLUDES= -I$(top_srcdir) -I$(top_srcdir)/kalarm \
	-I$(top_srcdir)/kalarm/widgets \
	-I$(top_builddir)/libical/src \
	-I$(top_builddir)/libkdepim \
	-I$(top_srcdir)/libkdenetwork $(all_includes)

bin_PROGRAMS = kalarm
# Don't change the order of the kalarm_SOURCES files without first checking
# that it builds with --enable-final !!!
kalarm_SOURCES = birthdaydlg.cpp alarmevent.cpp editdlg.cpp alarmcalendar.cpp \
	daemongui.cpp main.cpp kalarmapp.cpp \
        mainwindowbase.cpp mainwindow.cpp \
	messagewin.cpp \
	preferences.cpp prefdlg.cpp \
	traywindow.cpp dcophandler.cpp alarmguiiface.skel \
	recurrenceedit.cpp deferdlg.cpp \
	fontcolour.cpp fontcolourbutton.cpp datetime.cpp soundpicker.cpp \
	alarmlistview.cpp kamail.cpp
kalarm_LDFLAGS = $(all_libraries) $(KDE_RPATH)
kalarm_LDADD = $(top_builddir)/kalarm/widgets/libwidgets.la \
	-lkabc $(LIB_KDEUI) \
	$(top_builddir)/libkcal/libkcal.la \
	$(top_builddir)/libkdenetwork/libkdenetwork.la \
	$(top_builddir)/kalarmd/libkalarmd.la \
	$(top_builddir)/libkdepim/libkdepim.la
noinst_HEADERS = kalarm.h kalarmapp.h mainwindowbase.h mainwindow.h \
	messagewin.h editdlg.h recurrenceedit.h deferdlg.h birthdaydlg.h \
	alarmevent.h alarmcalendar.h alarmlistview.h kamail.h \
	preferences.h prefdlg.h \
	traywindow.h daemongui.h dcophandler.h \
	fontcolour.h fontcolourbutton.h datetime.h soundpicker.h

METASOURCES = AUTO

alarmguiiface_DIR = $(top_srcdir)/kalarmd

autostart_DATA = kalarm.tray.desktop
autostartdir = $(prefix)/share/autostart

kdelnkdir = $(kde_appsdir)/Utilities/More
kdelnk_DATA = kalarm.desktop

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(kde_appsdir)/Applications
	$(INSTALL_DATA) $(srcdir)/uninstall.desktop $(DESTDIR)$(kde_appsdir)/Applications/kalarm.desktop

KDE_ICON = AUTO

messages: 
	$(XGETTEXT) *.h *.cpp widgets/*.h widgets/*.cpp -o $(podir)/kalarm.pot

#include ../admin/Doxyfile.am
