TARGET = gstrecsign
TARGETLIB = lib$(TARGET).so
CC = gcc
CFLAGS = -Wall -fPIC
LIBFLAGS = --shared $(shell pkg-config --cflags --libs gstreamer-1.0)
RM = rm
CP = cp
NOECHO = @

.PHONY: clean

$(TARGETLIB): $(TARGET).c
	$(CC) $< $(CFLAGS) -o $@ $(LIBFLAGS)

all: $(TARGETLIB)

clean:
	$(RM) $(TARGETLIB)

.ONESHELL:
install: $(TARGETLIB)
	if [ "$(shell uname -ms)" = "Linux x86_64" ]; then
	cp $(TARGETLIB) /usr/lib/x86_64-linux-gnu/gstreamer-1.0/
	fi
