#
# Copyright (C) 2025  Blaise Dias
# 
# This file is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
# 
# It is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this file.  If not, see <http://www.gnu.org/licenses/>.
#

TARG_ARCH=$(shell uname -m)
TARG_CF =
TARG_DEFS =
LIBDIRS =

PLATFORM=$(shell ./scripts/ispcp.sh)
ifeq ($(PLATFORM),piCorePlayer)
	INCLUDE_TSLIB = -I ./tcz-libts/libts-1.0/src
	TS_LIB_H = ./tcz-libts/libts-1.0/src/tslib.h
	TARG_DEFS = -D USE_TSLIB_FOR_TOUCH
else
	INCLUDE_TSLIB =
	TS_LIB_H = 
endif

DEFS = $(TARG_DEFS)
SRC = ./src
JSON-PARSER-SRC = ./json-parser
CITYHASH-SRC = ./cityhash-c
VUMETERS-SRC = ./vumeters
INCLUDES-JSON-PARSER = -I $(JSON-PARSER-SRC)
INCLUDES-CITYHASH = -I $(CITYHASH-SRC)
INCLUDES = -I $(SRC) $(INCLUDES-JSON-PARSER) $(INCLUDES-CITYHASH) $(INCLUDE_TSLIB)

BIN_DIR = ./bin
OBJS_DIR = ./obj
LIB_DIR = ./lib
LIBS = -lm -lSDL2 -lSDL2_image -lSDL2_ttf -lts 

#SANITIZE =  -fsanitize=safe-stack
#SANITIZE =  -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined
#SANITIZE =  -fsanitize=memory -fno-omit-frame-pointer -fsanitize=undefined
SANITIZE = 
GLOBAL_DEPS = ./Makefile
CF = -Wall -g $(TARG_CF) $(DEFS) $(SANITIZE)
CCP = g++
CC = gcc
CF_PIC = $(CF) -fpic
CF_SHARED = $(CF) -fpic -shared

SHARED_OBJS = $(LIB_DIR)/Chevrons.so $(LIB_DIR)/PurpleTastic.so $(LIB_DIR)/TubeD.so
SQVUMETER_OBJS = \
		  $(OBJS_DIR)/sqvumeter.o \
		  $(OBJS_DIR)/application.o \
   		  $(OBJS_DIR)/util.o $(OBJS_DIR)/widgets.o $(OBJS_DIR)/actions.o \
   		  $(OBJS_DIR)/vumeter_widget.o $(OBJS_DIR)/vumeter_util.o $(OBJS_DIR)/visualizer.o $(OBJS_DIR)/vis_vumeter.o \
   		  $(OBJS_DIR)/json.o $(OBJS_DIR)/widgets_json.o \
   		  $(OBJS_DIR)/platform_linux.o $(OBJS_DIR)/logging.o \
   		  $(OBJS_DIR)/city.o $(OBJS_DIR)/texture_cache.o \
		  $(OBJS_DIR)/touch_screen.o \
		  $(OBJS_DIR)/touch_screen_sdl2.o \
   		  $(OBJS_DIR)/timing.o

all: $(BIN_DIR)/sqvumeter $(SHARED_OBJS) \
	$(BIN_DIR)/test_tcache \
	$(BIN_DIR)/test_widgets_json \
	$(BIN_DIR)/test_touch

.PHONY: clean

clean:
	rm -f $(OBJS_DIR)/*
	rm -f $(BIN_DIR)/*
	rm -f $(LIB_DIR)/*

#{
Makefile.deps: \
		$(SRC)/*.c $(JSON-PARSER-SRC)/*.c $(CITYHASH-SRC)/*.c $(VUMETERS-SRC)/*.c \
		$(SRC)/*.h $(JSON-PARSER-SRC)/*.h $(CITYHASH-SRC)/*.h \
		./scripts/mkdeps.py $(GLOBAL_DEPS)
	./scripts/mkdeps.py '$(SRC)' '$(JSON-PARSER-SRC)' '$(CITYHASH-SRC)' '$(VUMETERS-SRC)' -o '$(OBJS_DIR)' -g '$(GLOBAL_DEPS)'

include Makefile.deps
#}
# Alternative makefile way of doing above.
#
#https://www.gnu.org/software/make/manual/make.html#Automatic-Prerequisites
#%.d: %.c
#        @set -e; rm -f $@; \
#         $(CC) -M $(CPPFLAGS) $< > $@.$$$$; \
#         sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
#         rm -f $@.$$$$
#
#include $(sources:.c=.d)
#
$(OBJS_DIR)/%.o: $(SRC)/%.cpp | $(OBJS_DIR)
	$(CCP) $(CF) -c -o $(@) $< $(INCLUDES)

$(OBJS_DIR)/%.o: $(SRC)/%.c | $(OBJS_DIR)
	$(CC) $(CF) -c -o $(@) $< $(INCLUDES)

$(OBJS_DIR)/%.o: $(JSON-PARSER-SRC)/%.cpp | $(OBJS_DIR)
	$(CCP) $(CF) -c -o $(@) $< $(INCLUDES-JSON-PARSER)

$(OBJS_DIR)/%.o: $(JSON-PARSER-SRC)/%.c | $(OBJS_DIR)
	$(CC) $(CF) -c -o $(@) $< $(INCLUDES-JSON-PARSER)

$(OBJS_DIR)/%.o: $(CITYHASH-SRC)/%.cpp | $(OBJS_DIR)
	$(CCP) $(CF) -c -o $(@) $< $(INCLUDES-CITYHASH)

$(OBJS_DIR)/%.o: $(CITYHASH-SRC)/%.c | $(OBJS_DIR)
	$(CC) $(CF) -c -o $(@) $< $(INCLUDES-CITYHASH)

$(BIN_DIR)/sqvumeter : $(SQVUMETER_OBJS) | $(BIN_DIR)
	$(CC) $(CF) -o $(@) $^ $(LIBDIRS) $(LIBS)

# dynamic libraries
$(OBJS_DIR)/%.o: $(VUMETERS-SRC)/%.c | $(OBJS_DIR)
	$(CC) $(CF_PIC) -c -o $(@) $< $(INCLUDES)

$(LIB_DIR)/%.so: $(OBJS_DIR)/%.o | $(LIB_DIR)
	$(CC) $(CF_SHARED) $(INCLUDES) $< -o $@

# test executables
# 1. texture cache
$(BIN_DIR)/test_tcache : $(OBJS_DIR)/test_tcache.o $(OBJS_DIR)/texture_cache.o $(OBJS_DIR)/logging.o $(OBJS_DIR)/city.o $(OBJS_DIR)/timing.o | $(BIN_DIR)
	$(CC) $(CF) -o $(@) $^ $(LIBDIRS) $(LIBS)

# 2. json parsing
TEST_WIDGETS_JSON_OBJS =  \
	$(OBJS_DIR)/widgets_json.o \
	$(OBJS_DIR)/json.o \
	$(OBJS_DIR)/logging.o \
	$(OBJS_DIR)/widgets.o \
	$(OBJS_DIR)/vumeter_widget.o \
	$(OBJS_DIR)/actions.o \
	$(OBJS_DIR)/util.o \
	$(OBJS_DIR)/vumeter_util.o \
	$(OBJS_DIR)/visualizer.o \
	$(OBJS_DIR)/vis_vumeter.o \
	$(OBJS_DIR)/city.o $(OBJS_DIR)/texture_cache.o \
	$(OBJS_DIR)/timing.o \
	$(OBJS_DIR)/platform_linux.o

$(BIN_DIR)/test_widgets_json : $(OBJS_DIR)/test_widgets_json.o $(TEST_WIDGETS_JSON_OBJS) | $(BIN_DIR)
	$(CC) $(CF) -o $(@) $^ $(LIBDIRS) $(LIBS)

# 3. touch screen
$(BIN_DIR)/test_touch: $(OBJS_DIR)/test_touch.o $(OBJS_DIR)/touch_screen.o $(OBJS_DIR)/timing.o
	$(CC) $(CF) -o $(@) $^ $(LIBDIRS) $(LIBS)

# directories
$(BIN_DIR):
	mkdir -p $@

$(OBJS_DIR):
	mkdir -p $@

$(LIB_DIR):
	mkdir -p $@
