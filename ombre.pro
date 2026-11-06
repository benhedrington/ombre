QT += core gui qml quick quickcontrols2 dbus

CONFIG += c++17 release
TARGET = ombre
TEMPLATE = app

# The dev build finds backend/gradgen.py in the source tree. The installed
# package puts it in /usr/share/ombre, which QStandardPaths finds first.
DEFINES += OMBRE_SOURCE_DIR=\\\"$$PWD\\\"

HEADERS += \
    src/backend.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
