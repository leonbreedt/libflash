SUBDIRS = . sdk

lib_LTLIBRARIES = libflash-1.0.la

flash_lib_public_headers = \
	flash.h \
	flash-common.h \
	flash-library.h \
	flash-file.h

flash_lib_internal_headers = \
	flash-library-internal.h \
	flash-file-internal.h \
	flash-npapi.h \
	xembed.h \
	gtk2xtbin.h

flash_lib_sources = \
	flash-common.c \
	flash-library.c \
	flash-file.c \
	gtk2xtbin.c

flashincludedir = $(includedir)/flash-@FLASH_API_VERSION@/flash
flashinclude_HEADERS = $(flash_lib_public_headers)

EXTRA_DIST = $(flash_lib_internal_headers)

libflash_1_0_la_CFLAGS = -I$(srcdir)/sdk -I $(top_srcdir)/flash \
			 $(FLASH_LIB_CFLAGS)
libflash_1_0_la_LDFLAGS = $(FLASH_LIB_LIBS) -L/usr/X11R6/lib -lXt
libflash_1_0_la_SOURCES = $(flash_lib_sources)

bin_PROGRAMS = testflash
testflash_SOURCES = testflash.c
testflash_CFLAGS = $(FLASH_LIB_CFLAGS) -I$(top_srcdir)/flash
testflash_LDFLAGS = $(FLASH_LIB_LIBS)
testflash_LDADD = libflash-1.0.la
