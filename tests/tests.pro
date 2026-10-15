# Engine, recipe and history tests:
#   cd tests && qmake6 && make -j"$(nproc)" && ./tst_ghostframe
QT += testlib gui quick
QT -= widgets
CONFIG += testcase console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tst_ghostframe

include(../ghostframe.pri)

SOURCES += tst_ghostframe.cpp \
           ../src/imageeditor.cpp
HEADERS += ../src/imageeditor.h \
           ../src/previewprovider.h
