# Makefile for the Record Manager (RM) layer

# Compiler
CC = gcc
# Compiler flags
# We add -I. to find the headers (pf.h, pftypes.h) in the current directory
CFLAGS = -g -Wall -I. -I../pflayer

# Linker flags
LDFLAGS = -lm

# --- Source Files ---
# RM layer sources
RM_SRCS = rm.c
# Test program source
TEST_SRC = test_rm.c
# PF layer sources (relative paths)
PF_SRCS = ../pflayer/pf.c ../pflayer/buf.c ../pflayer/hash.c

# --- Object Files ---
# We will build all object files in the current (rmlayer) directory
RM_OBJS = rm.o
PF_OBJS = pf.o buf.o hash.o
TEST_OBJS = test_rm.o

# Target executable
TARGET = test_rm

# Default target
all: $(TARGET)

$(TARGET): $(RM_OBJS) $(TEST_OBJS) $(PF_OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(TEST_OBJS) $(RM_OBJS) $(PF_OBJS) $(LDFLAGS)

# --- Rules to build all objects ---

test_rm.o: test_rm.c rm.h pf.h pftypes.h
	$(CC) $(CFLAGS) -c $(TEST_SRC) -o test_rm.o

rm.o: rm.c rm.h pf.h pftypes.h
	$(CC) $(CFLAGS) -c $(RM_SRCS) -o rm.o

# Rules to build the PF layer objects from their source
pf.o: ../pflayer/pf.c pf.h pftypes.h
	$(CC) $(CFLAGS) -c ../pflayer/pf.c -o pf.o

buf.o: ../pflayer/buf.c pf.h pftypes.h
	$(CC) $(CFLAGS) -c ../pflayer/buf.c -o buf.o

hash.o: ../pflayer/hash.c pf.h pftypes.h
	$(CC) $(CFLAGS) -c ../pflayer/hash.c -o hash.o

clean:
	rm -f $(TARGET) *.o