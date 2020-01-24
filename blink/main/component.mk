#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
hid_device_le_prf.o: CFLAGS += -Wno-unused-const-variable 
