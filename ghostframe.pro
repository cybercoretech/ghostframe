QT += quick quickcontrols2 network qml
CONFIG += c++20
CONFIG += qtquickcompiler
TEMPLATE = app
TARGET = ghostframe

SOURCES += src/main.cpp \
           src/photoservice.cpp

HEADERS += src/photoservice.h

RESOURCES += qml.qrc

target.path = /usr/bin
INSTALLS += target
