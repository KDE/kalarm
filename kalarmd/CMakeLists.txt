kde4_header()

include_directories( ${CMAKE_SOURCE_DIR}/kalarm ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### next target ###############


########### next target ###############

set(kalarmd_SRCS 
	alarmdaemon.cpp 
	admain.cpp 
	adapp.cpp 
	adcalendar.cpp 
	adconfigdata.cpp 
	clientinfo.cpp 
)

kde4_add_dcop_skels(kalarmd_SRCS alarmdaemoniface.h alarmguiiface.h alarmdaemoniface.h )

kde4_automoc(${kalarmd_SRCS})

kde4_add_executable(kalarmd ${kalarmd_SRCS})

target_link_libraries(kalarmd  ${QT_AND_KDECORE_LIBS} )

install_targets(/bin kalarmd )


########### install files ###############

install_files( ${APPLNK_INSTALL_DIR}/.hidden FILES kalarmd.desktop )
install_files( /share/autostart FILES kalarmd.autostart.desktop )

kde4_footer()



#original Makefile.am contents follow:

#INCLUDES= -I$(top_srcdir) -I$(top_srcdir)/kalarm $(all_includes)
#
#noinst_LTLIBRARIES = libkalarmd.la
#
#libkalarmd_la_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#libkalarmd_la_LIBADD  = $(LIB_KDECORE)
#libkalarmd_la_SOURCES =  alarmdaemoniface.stub alarmguiiface.stub
#
#bin_PROGRAMS = kalarmd
#
#kalarmd_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#kalarmd_LDADD = $(LIB_KDEUI) $(top_builddir)/libkcal/libkcal.la libkalarmd.la
#kalarmd_SOURCES = alarmdaemon.cpp admain.cpp adapp.cpp \
#        adcalendar.cpp adconfigdata.cpp clientinfo.cpp \
#        alarmdaemoniface.skel
#kalarmd_COMPILE_FIRST = alarmguiiface_stub.h
#
#noinst_HEADERS = adapp.h alarmdaemon.h alarmdaemoniface.h alarmguiiface.h \
#	adcalendar.h adconfigdata.h clientinfo.h kalarmd.h
#
#METASOURCES = AUTO
# 
#autostart_DATA = kalarmd.autostart.desktop
#autostartdir = $(datadir)/autostart
#
#app_DATA = kalarmd.desktop
#appdir = $(kde_appsdir)/.hidden
#
