CC = gcc
CPP = g++
RM = rm

TARGET = route
CFLAGS += -O2 -Wall
DEFINES = -DVERSION=\"0.2\" -D_DEBUG=0 \
 -DDBTABLE=\"sf_streets\" -DDBUSER=\"postgres\"\
 -DDBNAME=\"sf_streets\" -DDBHOST=\"localhost\"\
 -DNAMECOL=\"fullname\" -DFACILITYCOL=\"class\"\
 -DLENCOL=\"length\" -DBIKELNCOL=\"bikelane\"
# routing:ridesf contains data from the city (all streetnames are all caps
 #-DDBTABLE=\"ridesf\" -DDBUSER=\"postgres\" -DDBNAME=\"routing\" -DDBHOST=\"localhost\"\
 #-DNAMECOL=\"fullname\" -DFACILITYCOL=\"class\" -DLENCOL=\"length\"

INCLUDE_PATHS += -I.

C_SRC = route.c 

OBJECTS = $(C_SRC:%.c=%.o)

LIBS = -lm -lpq -lcgi

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDE_PATHS) -o $@ -c $<

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o $(TARGET).cgi

install: $(TARGET)
	sudo cp $(TARGET).cgi /var/www/cgi-bin

archive: $(TARGET)
	tar czvf route.tar.gz Makefile route.c route.o route.cgi

clean:
	$(RM) -f *.o
	$(RM) -f $(TARGET).cgi

