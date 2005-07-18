INCLUDES = -I$(top_srcdir)/kalarm -I$(top_srcdir) $(all_includes)

noinst_LTLIBRARIES = libkalarm.la

libkalarm_la_METASOURCES = AUTO

libkalarm_la_SOURCES = \
	buttongroup.cpp checkbox.cpp colourcombo.cpp colourlist.cpp \
	combobox.cpp dateedit.cpp datetime.cpp label.cpp messagebox.cpp \
	pushbutton.cpp radiobutton.cpp timeedit.cpp timespinbox.cpp \
	timeperiod.cpp shellprocess.cpp slider.cpp spinbox.cpp spinbox2.cpp \
	lineedit.cpp synchtimer.cpp

noinst_HEADERS = \
	buttongroup.h checkbox.h colourcombo.h colourlist.h \
	combobox.h dateedit.h datetime.h label.h lineedit.h messagebox.h \
	pushbutton.h radiobutton.h timeedit.h timespinbox.h \
	timeperiod.h shellprocess.h slider.h spinbox.h spinbox2.h \
	synchtimer.h spinbox2private.h

DOXYGEN_REFERENCES=kdecore kdeui libkdepim
include $(top_srcdir)/admin/Doxyfile.am
