# Modified Makefile for AM Layer (Objective 3)

# Define directories
PFDIR = ../pflayer

# --- MODIFICATION ---
# Change PFOBJS to be local files, not files in ../pflayer
PFOBJS = pf.o buf.o hash.o

# Compiler and Flags
CC = gcc
CFLAGS = -g -Wall -I. -I$(PFDIR)
LIBS = -lm

# Source files (all .c files in this layer)
SRCS =  am.c \
	amfns.c \
	amglobals.c \
	aminsert.c \
	amprint.c \
	amscan.c \
	amsearch.c \
	amstack.c \
	misc.c \
	rm.c \
	test_am3.c

# Object files (all .o files to be built)
OBJS =  am.o \
	amfns.o \
	amglobals.o \
	aminsert.o \
	amprint.o \
	amscan.o \
	amsearch.o \
	amstack.o \
	misc.o \
	rm.o \
	test_am3.o

# The final program to build
TARGET = test_am3

# Default target
all: $(TARGET)

# --- MODIFICATION ---
# This rule now links the local PFOBJS (pf.o, buf.o, hash.o)
$(TARGET): $(OBJS) $(PFOBJS)
	@echo "Linking $(TARGET)..."
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(PFOBJS) $(LIBS)

# --- MODIFICATION ---
# REMOVED the old $(PFOBJS): rule that called make in ../pflayer

# --- Rules for local AM/RM/Test objects ---
am.o: am.c am.h pf.h
amfns.o: amfns.c am.h pf.h
amglobals.o: amglobals.c am.h
aminsert.o: aminsert.c am.h pf.h
amprint.o: amprint.c am.h pf.h
amscan.o: amscan.c am.h pf.h
amsearch.o: amsearch.c am.h pf.h
amstack.o: amstack.c am.h pf.h
misc.o: misc.c am.h pf.h testam.h
rm.o: rm.c rm.h pf.h pftypes.h
test_am3.o: test_am3.c am.h pf.h rm.h

# --- MODIFICATION ---
# ADDED rules to build pf.o, buf.o, and hash.o locally
# from the ../pflayer source files.
pf.o: ../pflayer/pf.c pf.h pftypes.h
	$(CC) $(CFLAGS) -c ../pflayer/pf.c -o pf.o

buf.o: ../pflayer/buf.c pf.h pftypes.h
	$(CC) $(CFLAGS) -c ../pflayer/buf.c -o buf.o

hash.o: ../pflayer/hash.c pf.h pftypes.h
	$(CC) $(CFLAGS) -c ../pflayer/hash.c -o hash.o

clean:
	@echo "Cleaning AM layer..."
	rm -f $(TARGET) $(OBJS) $(PFOBJS)