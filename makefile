include ../../config.mk
CC =g++
CFLAGS =-c -Wall -std=c++17 -I'./'
SOURCES =FsLib.cpp
OBJECTS =$(SOURCES:.cpp=.o)
LINKABLE =FsLib
ODIR =../../bin/unix/$(PLATFORM)/$(CONFIGURATION)/$(LINKABLE)

all: $(SOURCES) $(LINKABLE)

$(LINKABLE): $(OBJECTS)
	ar rc $(LINKABLE) $(OBJECTS)
	mkdir -p $(ODIR)
	mv $(LINKABLE) $(ODIR)

.cpp.o:
	$(CC) $(BASE_CFLAGS) $(CFLAGS) $< -o $@

clean:
	rm *.o
	rm $(FSLIB)

rebuild:
	clean all