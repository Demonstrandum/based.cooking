#!/usr/bin/make -f

# The following can should configured in config.h as well
ARTICLES_HTML ?= ./blog
ARTICLES_MARKDOWN ?= ./src
CACHE_FILE ?= ./.buildcache

PUBLIC ?= ./data
REMOTE ?= ./test_deploy

# C compilation
CC ?= cc
CLINKS ?= -lmarkdown
CGILINKS ?= -lfcgi
CFLAGS += -Os -std=c99 -Wall -Wpedantic -Wextra
CFILES := $(wildcard *.c)
OUT ?= ./bin
BINARY ?= based
CTARGET ?= $(OUT)/$(BINARY)
OBJS := $(patsubst %.c,$(OUT)/%.o,$(CFILES))

.PHONY: help init compile build deploy cgi clean

help:
	$(info make init|build|deploy|clean)

# to start a fresh project
init:
	mkdir -p $(ARTICLES_MARKDOWN) $(ARTICLES_HTML) data

$(OUT)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(CTARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(CTARGET) $(OBJS) $(CLINKS)

$(OUT):
	mkdir -p $(OUT)

$(ARTICLES_HTML):
	mkdir -p $(ARTICLES_HTML)

compile: $(OUT) $(CTARGET)

build: compile $(ARTICLES_HTML)
	mkdir -p $(ARTICLES_HTML)
	$(CTARGET) -s $(ARTICLES_MARKDOWN) -d $(ARTICLES_HTML) -q

deploy: build
	rsync -rLtz $(BLOG_RSYNC_OPTS) $(ARTICLES_HTML)/ $(PUBLIC)/ $(REMOTE)

cgi: fastcgi/searchcgi.c
	$(CC) $(CFLAGS) fastcgi/searchcgi.c -o fastcgi/search.fcgi $(CLINKS)

clean:
	rm -rf $(ARTICLES_HTML)/* $(OUT)

