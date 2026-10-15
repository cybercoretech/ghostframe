# Core sources shared by the app (ghostframe.pro) and the tests (tests/tests.pro).
INCLUDEPATH += $$PWD/src
CONFIG += c++20 link_pkgconfig
PKGCONFIG += vips-cpp
QT += concurrent gui

SOURCES += $$PWD/src/editrecipe.cpp \
           $$PWD/src/edithistory.cpp \
           $$PWD/src/imageengine.cpp \
           $$PWD/src/recipestore.cpp \
           $$PWD/src/fx.cpp \
           $$PWD/src/cubelut.cpp \
           $$PWD/src/cybergrid.cpp \
           $$PWD/src/looklibrary.cpp

HEADERS += $$PWD/src/editrecipe.h \
           $$PWD/src/edithistory.h \
           $$PWD/src/imageengine.h \
           $$PWD/src/recipestore.h \
           $$PWD/src/fx.h \
           $$PWD/src/cubelut.h \
           $$PWD/src/cybergrid.h \
           $$PWD/src/looklibrary.h

RESOURCES += $$PWD/cybergrid.qrc \
             $$PWD/looks.qrc
