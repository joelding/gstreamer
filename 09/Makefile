CC = gcc
RM = rm -f
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))
CFLAGS = -Wall `pkg-config --cflags gstreamer-1.0 gstreamer-audio-1.0, gstreamer-pbutils-1.0`
LFLAGS = `pkg-config --libs gstreamer-1.0 gstreamer-audio-1.0 gstreamer-pbutils-1.0`
TGTS = basic-tutorial-9

all : $(TGTS)

$(TGTS) : $(OBJS)
	$(CC) -o $(TGTS) $(OBJS) $(LFLAGS)

.c.o :
	$(CC) $(CFLAGS) -c $<

clean :
	$(RM) $(TGTS) $(OBJS)
