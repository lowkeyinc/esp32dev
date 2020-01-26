#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
*.o: CFLAGS += -Wno-unused-const-variable -Wno-dangling-else
