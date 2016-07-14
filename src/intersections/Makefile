CC = gcc
CPP = g++
RM = rm

TARGET = intersections 

CFLAGS += -Wall

DEFINES = -DVERSION=\"0.1\"

INCLUDE_PATHS += -I. \
		 -I.. \
		 -I../shapelib

C_SRC = intersections.c \
	../shapelib/shpopen.c \
	../shapelib/dbfopen.c

OBJECTS = $(C_SRC:%.c=%.o)

LIBS =

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDE_PATHS) -o $@ -c $<

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o $(TARGET)

clean:
	$(RM) -f *.o
	$(RM) -f $(TARGET)

