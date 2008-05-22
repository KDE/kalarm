#KDE_OPTIONS = nofinal

SUBDIRS = kalarmd lib pixmaps

INCLUDES= -I$(top_srcdir) -I$(top_srcdir)/kalarm \
	-I$(top_srcdir)/kalarm/lib \
	-I$(top_builddir)/libkcal/libical/src \
	-I$(top_srcdir)/libkmime \
	-I$(top_srcdir)/certmanager/lib $(all_includes)

bin_PROGRAMS = kalarm
# Don't change the order of the kalarm_SOURCES files without first checking
# that it builds with --enable-final !!!
kalarm_SOURCES = birthdaydlg.cpp main.cpp alarmevent.cpp editdlg.cpp \
	fontcolour.cpp soundpicker.cpp sounddlg.cpp alarmcalendar.cpp daemon.cpp undo.cpp \
	kalarmapp.cpp mainwindowbase.cpp mainwindow.cpp messagewin.cpp \
	preferences.cpp prefdlg.cpp kalarmiface.skel \
	traywindow.cpp dcophandler.cpp alarmguiiface.skel \
	recurrenceedit.cpp karecurrence.cpp deferdlg.cpp functions.cpp \
	fontcolourbutton.cpp alarmtimewidget.cpp \
	specialactions.cpp reminder.cpp startdaytimer.cpp \
	eventlistviewbase.cpp alarmlistview.cpp kamail.cpp timeselector.cpp \
	templatelistview.cpp templatepickdlg.cpp templatedlg.cpp \
	templatemenuaction.cpp latecancel.cpp repetition.cpp alarmtext.cpp \
	emailidcombo.cpp find.cpp pickfileradio.cpp calendarcompat.cpp
kalarm_LDFLAGS = $(all_libraries) $(KDE_RPATH)
if include_ARTS
ARTSLIB = -lartskde
endif
kalarm_LDADD = $(top_builddir)/kalarm/lib/libkalarm.la \
	$(top_builddir)/libkcal/libkcal.la \
	$(top_builddir)/libkmime/libkmime.la \
	$(top_builddir)/kalarm/kalarmd/libkalarmd.la \
	$(top_builddir)/libkdepim/libkdepim.la \
	$(top_builddir)/libkpimidentities/libkpimidentities.la \
	-lkabc -lkutils $(LIB_KDEUI) $(ARTSLIB)
noinst_HEADERS = alarmcalendar.h alarmevent.h alarmlistview.h alarmtext.h \
	alarmtimewidget.h birthdaydlg.h daemon.h dcophandler.h deferdlg.h \
	editdlg.h editdlgprivate.h emailidcombo.h eventlistviewbase.h find.h \
	fontcolour.h fontcolourbutton.h functions.h kalarm.h kalarmapp.h \
	kamail.h karecurrence.h latecancel.h mainwindow.h mainwindowbase.h \
	messagewin.h pickfileradio.h prefdlg.h preferences.h recurrenceedit.h \
	recurrenceeditprivate.h reminder.h repetition.h sounddlg.h \
	soundpicker.h specialactions.h startdaytimer.h templatedlg.h \
	templatelistview.h templatemenuaction.h templatepickdlg.h timeselector.h \
	traywindow.h undo.h calendarcompat.h

METASOURCES = AUTO

alarmguiiface_DIR = $(top_srcdir)/kalarm/kalarmd

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
	$(EXTRACTRC) *.rc >> rc.cpp
	$(XGETTEXT) *.h *.cpp lib/*.h lib/*.cpp kalarmd/*.cpp -o $(podir)/kalarm.pot

DOXYGEN_REFERENCES = libkcal
include $(top_srcdir)/admin/Doxyfile.am

calendarcompat.o alarmcalendar.o: ../libkcal/libical/src/libical/ical.h
daemon.o: kalarmd/alarmdaemoniface_stub.h

