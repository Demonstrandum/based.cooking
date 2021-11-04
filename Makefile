#!/usr/bin/make -f

# The following can be configured in config.h as well
ARTICLES_DST ?= ./blog
ARTICLES_SRC ?= ./src
PUBLIC ?= ./data

# C compilation
CC ?= gcc
CLINKS ?= -lmarkdown
CFLAGS ?= -O3 -std=c89 -Wall -Wpedantic -Wextra
CFILES := $(wildcard *.c)
OUT ?= ./bin
BINARY ?= based
CTARGET ?= $(OUT)/$(BINARY)
OBJS := $(patsubst %.c,$(OUT)/%.o,$(CFILES))

.PHONY: help init compile build deploy clean

help:
	$(info make init|build|deploy|clean)

init:
	# to start a fresh project
	mkdir -p $(ARTICLES_SRC) data

$(OUT)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(CLINKS)

$(CTARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(CTARGET) $(OBJS) $(CLINKS)

$(OUT):
	mkdir -p $(OUT)

compile: $(OUT) $(CTARGET)

build: compile
	mkdir -p $(ARTICLES_DST)
	$(CTARGET) -s $(ARTICLES_SRC) -d $(ARTICLES_DST) -q

deploy: build
	rsync -rLtz $(BLOG_RSYNC_OPTS) $(ARTICLES_DST)/ $(PUBLIC)/ $(REMOTE)

clean:
	rm -rf $(ARTICLES_DST) $(OUT)

