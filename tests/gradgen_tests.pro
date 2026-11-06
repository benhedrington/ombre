QT += core gui quick quickcontrols2 testlib

CONFIG += c++17 testcase
TARGET = gradgen_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h
SOURCES += gradgen_tests.cpp ../src/backend.cpp
