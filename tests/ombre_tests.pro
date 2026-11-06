QT += core gui quick quickcontrols2 testlib

CONFIG += c++17 testcase
TARGET = ombre_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h
SOURCES += ombre_tests.cpp ../src/backend.cpp
