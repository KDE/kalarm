SUBDIRS = widgets pixmaps

INCLUDES= -I$(top_srcdir) -I$(top_srcdir)/kalarm \
	-I$(top_srcdir)/kalarm/widgets \
	-I$(top_builddir)/libical/src \
	-I$(top_srcdir)/libkmime \
	-I$(top_srcdir)/certmanager/lib $(all_includes)

bin_PROGRAMS = kalarm
# Don't change the order of the kalarm_SOURCES files without first checking
# that it builds with --enable-final !!!
kalarm_SOURCES = birthdaydlg.cpp alarmevent.cpp editdlg.cpp alarmcalendar.cpp \
	soundpicker.cpp daemon.cpp main.cpp kalarmapp.cpp \
        mainwindowbase.cpp mainwindow.cpp messagewin.cpp \
	preferences.cpp prefdlg.cpp kalarmiface.skel \
	traywindow.cpp dcophandler.cpp alarmguiiface.skel \
	recurrenceedit.cpp deferdlg.cpp functions.cpp \
	fontcolour.cpp fontcolourbutton.cpp alarmtimewidget.cpp \
	specialactions.cpp reminder.cpp synchtimer.cpp \
	eventlistviewbase.cpp alarmlistview.cpp kamail.cpp timeselector.cpp \
	templatelistview.cpp templatepickdlg.cpp templatedlg.cpp \
	latecancel.cpp repetition.cpp alarmtext.cpp emailidcombo.cpp
kalarm_LDFLAGS = $(all_libraries) $(KDE_RPATH)
if include_ARTS
ARTSLIB = -lartskde
endif
kalarm_LDADD = $(top_builddir)/kalarm/widgets/libwidgets.la \
	$(top_builddir)/libkcal/libkcal.la \
	$(top_builddir)/libkmime/libkmime.la \
	$(top_builddir)/kalarmd/libkalarmd.la \
	$(top_builddir)/libkdepim/libkdepim.la \
	$(top_builddir)/libkpimidentities/libkpimidentities.la \
	-lkabc $(LIB_KDEUI) $(ARTSLIB)
noinst_HEADERS = alarmcalendar.h alarmevent.h alarmlistview.h alarmtext.h \
	alarmtimewidget.h birthdaydlg.h daemon.h dcophandler.h deferdlg.h \
	editdlg.h editdlgprivate.h emailidcombo.h eventlistviewbase.h \
	fontcolour.h fontcolourbutton.h functions.h kalarm.h kalarmapp.h \
	kamail.h latecancel.h mainwindow.h mainwindowbase.h messagewin.h \
	prefdlg.h preferences.h recurrenceedit.h recurrenceeditprivate.h \
	reminder.h repetition.h soundpicker.h \
	specialactions.h synchtimer.h templatedlg.h templatelistview.h \
	templatepickdlg.h timeselector.h traywindow.h
kalarm_COMPILE_FIRST = ../kalarmd/alarmdaemoniface_stub.h

METASOURCES = AUTO

alarmguiiface_DIR = $(top_srcdir)/kalarmd

autostart_DATA = kalarm.tray.desktop
autostartdir = $(datadir)/autostart

xdg_apps_DATA = kalarm.desktop

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(kde_appsdir)/Applications
	$(INSTALL_DATA) $(srcdir)/uninstall.desktop $(DESTDIR)$(kde_appsdir)/Applications/kalarm.desktop

KDE_ICON = AUTO


rcdir = $(kde_datadir)/kalarm
rc_DATA = kalarmui.rc

messages: rc.cpp
	$(XGETTEXT) *.h *.cpp widgets/*.h widgets/*.cpp -o $(podir)/kalarm.pot

#include ../admin/Doxyfile.am
