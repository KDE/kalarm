kde4_header()

MESSAGE(STATUS "readd kalarmd !")
#add_subdirectory( kalarmd ) 
add_subdirectory( lib ) 
add_subdirectory( pixmaps ) 

include_directories( ${CMAKE_SOURCE_DIR}/kalarm ${CMAKE_SOURCE_DIR}/kalarm/lib ${CMAKE_SOURCE_DIR}/libkmime ${CMAKE_SOURCE_DIR}/libkleo ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### next target ###############
set(libkalarm_SRCS
	${CMAKE_SOURCE_DIR}/kalarm/lib/buttongroup.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/checkbox.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/colourcombo.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/colourlist.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/combobox.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/dateedit.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/datetime.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/label.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/messagebox.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/pushbutton.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/radiobutton.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/timeedit.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/timespinbox.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/timeperiod.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/shellprocess.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/slider.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/spinbox.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/spinbox2.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/lineedit.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/synchtimer.cpp
)

set(kalarm_SRCS ${libkalarm_SRCS}
   birthdaydlg.cpp 
   main.cpp 
   alarmevent.cpp 
   editdlg.cpp 
   soundpicker.cpp 
   sounddlg.cpp 
   alarmcalendar.cpp 
   daemon.cpp 
   undo.cpp 
   kalarmapp.cpp 
   mainwindowbase.cpp 
   mainwindow.cpp 
   messagewin.cpp 
   preferences.cpp 
   prefdlg.cpp 
   traywindow.cpp 
   dcophandler.cpp 
   recurrenceedit.cpp 
   karecurrence.cpp 
   deferdlg.cpp 
   functions.cpp 
   fontcolour.cpp 
   fontcolourbutton.cpp 
   alarmtimewidget.cpp 
   specialactions.cpp 
   reminder.cpp 
   startdaytimer.cpp 
   eventlistviewbase.cpp 
   alarmlistview.cpp 
   kamail.cpp 
   timeselector.cpp 
   templatelistview.cpp 
   templatepickdlg.cpp 
   templatedlg.cpp 
   templatemenuaction.cpp 
   latecancel.cpp 
   repetition.cpp 
   alarmtext.cpp 
   emailidcombo.cpp 
   find.cpp 
   pickfileradio.cpp 
   calendarcompat.cpp )

kde4_automoc(${kalarm_SRCS})

kde4_add_dcop_skels(kalarm_SRCS kalarmiface.h alarmguiiface.h )

kde4_add_executable(kalarm ${kalarm_SRCS})

target_link_libraries(kalarm  ${QT_AND_KDECORE_LIBS} kalarm kcal kmime kalarmd kdepim kpimidentities kabc kutils kdeui )

install_targets(/bin kalarm )


########### install files ###############

install_files( ${XDG_APPS_DIR} FILES kalarm.desktop )
install_files( /share/autostart FILES kalarm.tray.desktop )
install_files( ${DATA_INSTALL_DIR}/kalarm FILES kalarmui.rc )

kde4_install_icons( ${ICON_INSTALL_DIR}  hicolor )

kde4_create_doxygen_docs( REFERENCES libkcal  )

kde4_footer()



#original Makefile.am contents follow:

##KDE_OPTIONS = nofinal
#
#SUBDIRS = kalarmd lib pixmaps
#
#INCLUDES= -I$(top_srcdir) -I$(top_srcdir)/kalarm \
#	-I$(top_srcdir)/kalarm/lib \
#	-I$(top_builddir)/libkcal/libical/src \
#	-I$(top_srcdir)/libkmime \
#	-I$(top_srcdir)/libkleo $(all_includes)
#
#bin_PROGRAMS = kalarm
## Don't change the order of the kalarm_SOURCES files without first checking
## that it builds with --enable-final !!!
#kalarm_SOURCES = birthdaydlg.cpp main.cpp alarmevent.cpp editdlg.cpp \
#	soundpicker.cpp sounddlg.cpp alarmcalendar.cpp daemon.cpp undo.cpp \
#	kalarmapp.cpp mainwindowbase.cpp mainwindow.cpp messagewin.cpp \
#	preferences.cpp prefdlg.cpp kalarmiface.skel \
#	traywindow.cpp dcophandler.cpp alarmguiiface.skel \
#	recurrenceedit.cpp karecurrence.cpp deferdlg.cpp functions.cpp \
#	fontcolour.cpp fontcolourbutton.cpp alarmtimewidget.cpp \
#	specialactions.cpp reminder.cpp startdaytimer.cpp \
#	eventlistviewbase.cpp alarmlistview.cpp kamail.cpp timeselector.cpp \
#	templatelistview.cpp templatepickdlg.cpp templatedlg.cpp \
#	templatemenuaction.cpp latecancel.cpp repetition.cpp alarmtext.cpp \
#	emailidcombo.cpp find.cpp pickfileradio.cpp calendarcompat.cpp
#kalarm_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#if include_ARTS
#ARTSLIB = -lartskde
#endif
#kalarm_LDADD = $(top_builddir)/kalarm/lib/libkalarm.la \
#	$(top_builddir)/libkcal/libkcal.la \
#	$(top_builddir)/libkmime/libkmime.la \
#	$(top_builddir)/kalarm/kalarmd/libkalarmd.la \
#	$(top_builddir)/libkdepim/libkdepim.la \
#	$(top_builddir)/libkpimidentities/libkpimidentities.la \
#	-lkabc -lkutils $(LIB_KDEUI) $(ARTSLIB)
#noinst_HEADERS = alarmcalendar.h alarmevent.h alarmlistview.h alarmtext.h \
#	alarmtimewidget.h birthdaydlg.h daemon.h dcophandler.h deferdlg.h \
#	editdlg.h editdlgprivate.h emailidcombo.h eventlistviewbase.h find.h \
#	fontcolour.h fontcolourbutton.h functions.h kalarm.h kalarmapp.h \
#	kamail.h karecurrence.h latecancel.h mainwindow.h mainwindowbase.h \
#	messagewin.h pickfileradio.h prefdlg.h preferences.h recurrenceedit.h \
#	recurrenceeditprivate.h reminder.h repetition.h sounddlg.h \
#	soundpicker.h specialactions.h startdaytimer.h templatedlg.h \
#	templatelistview.h templatemenuaction.h templatepickdlg.h timeselector.h \
#	traywindow.h undo.h calendarcompat.h
#
#METASOURCES = AUTO
#
#alarmguiiface_DIR = $(top_srcdir)/kalarm/kalarmd
#
#autostart_DATA = kalarm.tray.desktop
#autostartdir = $(datadir)/autostart
#
#xdg_apps_DATA = kalarm.desktop
#
#install-data-local:
#	$(mkinstalldirs) $(DESTDIR)$(kde_appsdir)/Applications
#	$(INSTALL_DATA) $(srcdir)/uninstall.desktop $(DESTDIR)$(kde_appsdir)/Applications/kalarm.desktop
#
#KDE_ICON = AUTO
#
#
#rcdir = $(kde_datadir)/kalarm
#rc_DATA = kalarmui.rc
#
#messages: rc.cpp
#	$(EXTRACTRC) *.rc >> rc.cpp
#	$(XGETTEXT) *.h *.cpp lib/*.h lib/*.cpp kalarmd/*.cpp -o $(podir)/kalarm.pot
#
#DOXYGEN_REFERENCES = libkcal
#include $(top_srcdir)/admin/Doxyfile.am
