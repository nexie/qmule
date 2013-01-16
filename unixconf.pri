# Generated by the configure file
include(conf.pri)

# COMPILATION SPECIFIC
!nox:dbus {
  QT += dbus
}

QMAKE_CXXFLAGS += -Wformat -Wformat-security -DBOOST_FILESYSTEM_VERSION=2 -DBOOST_ASIO_HEADER_ONLY
QMAKE_LFLAGS_APP += -rdynamic
CONFIG += link_pkgconfig
PKGCONFIG += libtorrent-rasterbar
INCLUDEPATH += $$(LIBED2K_ROOT)/include
INCLUDEPATH += $$(BOOST_ROOT)/include
INCLUDEPATH += $$(LIBTORRENT_ROOT)/include
LIBS += -L$$(LIBED2K_ROOT)/lib -led2k -lboost_thread -lpthread -lboost_system -lboost_filesystem -lrt
LIBS += -L$$(BOOST_ROOT)/lib
LIBS += -L$$(LIBTORRENT_ROOT)/lib

DEFINES += LIBED2K_USE_BOOST_DATE_TIME

CONFIG(debug, debug|release) {
  DEFINES += TORRENT_DEBUG
  DEFINES += LIBED2K_DEBUG
} else {
  DEFINES += NDEBUG
 }

DEFINES += "BUILDDATE='\"$$system(date -R)\"'"
DEFINES += BOOST_EXCEPTION_DISABLE


# Man page
nox {
    man.files = ../doc/qbittorrent-nox.1
} else {
    man.files = ../doc/qbittorrent.1
}

man.path = $$MANPREFIX/man/man1/
INSTALLS += man

# INSTALL
target.path = $$PREFIX/bin/
INSTALLS += target

dbus {
  include(src/qtnotify/qtnotify.pri)
}

!nox {
  # DEFINE added by configure
  contains(DEFINES, WITH_GEOIP_EMBEDDED) {
    message("You chose to embed GeoIP database in qMule executable.")
  }
}
