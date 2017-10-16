SOURCES = ./src/
INCLUDES = ../include/

all:    kelvin-mmap-all kelvin-read-mmap-all kelvin-log-mmap-all kelvin-shm-all kelvin-read-shm-all kelvin-log-shm-all
all-u3-mmap: kelvin-mmap kelvin-read-mmap kelvin-log-mmap
all-u3-shm:  kelvin-shm kelvin-read-shm kelvin-log-shm

kelvin-mmap-all: $(SOURCES)kelvin-u3.c
	gcc -lm -llabjackusb u3.o -o kelvin-u3-mmap $(SOURCES)kelvin-u3.c

kelvin-read-mmap-all: $(SOURCES)kelvin-read.c
	gcc -o kelvin-read-mmap $(SOURCES)kelvin-read.c

kelvin-log-mmap-all: $(SOURCES)kelvin-log.c
	gcc  -Wall -o kelvin-log-mmap $(SOURCES)kelvin-log.c

kelvin-shm-all: $(SOURCES)kelvin-u3-shm.c
	gcc -lm -llabjackusb u3.o -o kelvin-u3-shm $(SOURCES)kelvin-u3-shm.c

kelvin-read-shm-all: $(SOURCES)kelvin-read-shm.c
	gcc -o kelvin-read-shm $(SOURCES)kelvin-read-shm.c

kelvin-log-shm-all: $(SOURCES)kelvin-log-shm.c
	gcc -Wall -o kelvin-log-shm $(SOURCES)kelvin-log-shm.c

##Method to make only one executable.

kelvin-mmap: $(SOURCES)kelvin-u3.c
	gcc -lm -llabjackusb u3.o -o kelvin-u3-mmap $(SOURCES)kelvin-u3.c

kelvin-read-mmap: $(SOURCES)kelvin-read.c
	gcc  -o kelvin-read $(SOURCES)kelvin-read.c

kelvin-log-mmap: $(SOURCES)kelvin-log.c
	gcc  -Wall -o kelvin-log $(SOURCES)kelvin-log.c

kelvin-shm: $(SOURCES)kelvin-u3-shm.c
	gcc -lm -llabjackusb u3.o -o kelvin-u3 $(SOURCES)kelvin-u3-shm.c

kelvin-read-shm: $(SOURCES)kelvin-read-shm.c
	gcc -o kelvin-read $(SOURCES)kelvin-read-shm.c

kelvin-log-shm: $(SOURCES)kelvin-log-shm.c
	gcc -Wall -o kelvin-log $(SOURCES)kelvin-log-shm.c

clean:
	rm kelvin*
