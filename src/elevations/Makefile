CC = gcc
CPP = g++
RM = rm

TARGET = elevations 
CFLAGS += -g -Wall
DEFINES = -DVERSION=\"0.1\"
SHPLIB_PATH = ../shapelib

INCLUDE_PATHS += -I. \
		 -I$(SHPLIB_PATH) 

C_SRC = elevations.c \
	$(SHPLIB_PATH)/shpopen.c \
	$(SHPLIB_PATH)/dbfopen.c

OBJECTS = $(C_SRC:%.c=%.o)

LIBS = -lcurl

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDE_PATHS) -o $@ -c $<

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o $(TARGET)

clean:
	$(RM) -f *.o
	$(RM) -f $(TARGET)

