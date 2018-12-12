CONVERTER_NAME = zfs2ceph
CONVERTER_SOURCES = src/zfs2ceph.c

CCFLAGS = -Wall -g -O3

.PHONY: all clean

all:
	$(CC) $(CCFLAGS) -o $(CONVERTER_NAME) $(CONVERTER_SOURCES)

clean:
	rm -f $(CONVERTER_NAME)