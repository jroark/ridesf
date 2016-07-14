CC = gcc
CPP = g++
RM = rm

TARGET = findnames 
CFLAGS += -g -Wall
DEFINES = -DVERSION=\"0.1\"
SHPLIB_PATH = ../shapelib
LIBXML_PATH = /usr/include/libxml2

INCLUDE_PATHS += -I. \
		 -I$(SHPLIB_PATH) \
		 -I$(LIBXML_PATH)

C_SRC = findnames.c \
	$(SHPLIB_PATH)/shpopen.c \
	$(SHPLIB_PATH)/dbfopen.c

OBJECTS = $(C_SRC:%.c=%.o)

LIBS = -lcurl -lxml2 -lm

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDE_PATHS) -o $@ -c $<

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o $(TARGET)

clean:
	$(RM) -f *.o
	$(RM) -f $(TARGET)

