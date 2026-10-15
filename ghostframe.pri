# Core sources shared by the app (ghostframe.pro) and the tests (tests/tests.pro).
INCLUDEPATH += $$PWD/src
CONFIG += c++20 link_pkgconfig
PKGCONFIG += vips-cpp
QT += concurrent

SOURCES += $$PWD/src/editrecipe.cpp \
           $$PWD/src/edithistory.cpp \
           $$PWD/src/imageengine.cpp

HEADERS += $$PWD/src/editrecipe.h \
           $$PWD/src/edithistory.h \
           $$PWD/src/imageengine.h
