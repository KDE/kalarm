# set the include path for X, qt and KDE and calendar library
INCLUDES= $(all_includes) -I$(top_srcdir)/libkcal

bin_PROGRAMS = kalarm
kalarm_SOURCES = main.cpp msgevent.cpp messagewin.cpp fontcolour.cpp fontchooser.cpp colourcombo.cpp prefsettings.cpp prefs.cpp prefdlg.cpp mainwindow.cpp kalarmapp.cpp editdlg.cpp
kalarm_LDFLAGS = $(all_libraries) $(KDE_RPATH)
kalarm_LDADD   = $(LIB_KDEUI) $(top_builddir)/libkcal/libkcal.la
noinst_HEADERS = kalarm.h kalarmapp.h mainwindow.h messagewin.h editdlg.h \
	msgevent.h fontcolour.h fontchooser.h colourcombo.h \
	prefs.h prefsettings.h prefdlg.h

METASOURCES = AUTO

SUBDIRS = pixmaps 

install-data-local:
	$(mkinstalldirs) $(kde_appsdir)/Applications/
	$(INSTALL_DATA) $(srcdir)/kalarm.desktop $(kde_appsdir)/Applications/kalarm.desktop

uninstall-local:
	-rm -f $(kde_appsdir)/Applications/kalarm.desktop

appicondir = $(kde_datadir)/kalarm/icons
appicon_ICON = AUTO

messages: 
	LIST=`find . -name \*.h -o -name \*.hh -o -name \*.H -o -name \*.hxx -o -name \*.hpp -o -name \*.cpp -o -name \*.cc -o -name \*.cxx -o -name \*.ecpp -o -name \*.C`; \
	if test -n "$$LIST"; then \
	 $(XGETTEXT) $$LIST -o $(podir)/kalarm.pot; \
	fi
