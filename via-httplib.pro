# Copyright (c) 2013-2015 Ken Barker
# (ken dot barker at via-technology dot co dot uk)
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# Check that VIAHTTPLIB has been set
# Note: isEmpty doesn't work on environ var directly so put it in a local first
VIAHTTPLIB = $$(VIAHTTPLIB)
isEmpty(VIAHTTPLIB) {
  error("The VIAHTTPLIB environment variable has not been set. Please set it to the location of the via-httplib libraries")
} else {
  message(Using via-httplib from: $$VIAHTTPLIB)
}

win32 {
  # Ensure that the BOOST_ROOT environment variable has been set
  BOOST_ROOT = $$(BOOST_ROOT)
  isEmpty(BOOST_ROOT) {
    error("Please set BOOST_ROOT to the location of the Boost libraries")
  } else {
    message(Using Boost from: $$BOOST_ROOT)
  }
}

TEMPLATE = lib
CONFIG -= app_bundle
CONFIG -= qt
CONFIG += c++14
CONFIG += thread
CONFIG += shared
CONFIG += separate_debug_info

# Compiler options
*-g++* {
  message(Setting flags for GCC or MinGw)

  QMAKE_CXXFLAGS += -Wall
  QMAKE_CXXFLAGS += -Wextra
  QMAKE_CXXFLAGS += -Wpedantic

  win32 {
    # Set debug options for MinGw in QtCreator
    QMAKE_CXXFLAGS_DEBUG = -O0
    QMAKE_CXXFLAGS_DEBUG += -gdwarf-3
  }
}

*-clang* {
  message(Setting flags for clang)
  QMAKE_CXXFLAGS += -stdlib=libc++
}

include (via-httplib.pri)

SRC_DIR = $$VIAHTTPLIB/src

SOURCES += $${SRC_DIR}/via/http/character.cpp
SOURCES += $${SRC_DIR}/via/http/chunk.cpp
SOURCES += $${SRC_DIR}/via/http/header_field.cpp
SOURCES += $${SRC_DIR}/via/http/headers.cpp
SOURCES += $${SRC_DIR}/via/http/request_method.cpp
SOURCES += $${SRC_DIR}/via/http/request.cpp
SOURCES += $${SRC_DIR}/via/http/response_status.cpp
SOURCES += $${SRC_DIR}/via/http/response.cpp

CONFIG(release, debug|release) {
  DESTDIR = $${OUT_PWD}/release
} else {
  DESTDIR = $${OUT_PWD}/debug
}

OBJECTS_DIR = $${DESTDIR}/obj

# To run install after a build:
windows {
  contains(QMAKE_TARGET.arch, x86_64) {
    DLL_DIR = /Windows/system
  } else {
    DLL_DIR = /Windows/system32
  }
} else {
  macx {
    DLL_DIR = /usr/local/lib
  } else {
    contains(QMAKE_TARGET.arch, x86_64) {
      DLL_DIR = /usr/lib64
    } else {
      DLL_DIR = /usr/lib
    }
  }
}

target.path = $$DLL_DIR
INSTALLS += target
