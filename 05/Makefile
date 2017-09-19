CC = gcc
RM = rm -f
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))
CFLAGS = -Wall `pkg-config --cflags gstreamer-1.0 gstreamer-video-1.0 gtk+-3.0`
LFLAGS = `pkg-config --libs gstreamer-1.0 gstreamer-video-1.0 gtk+-3.0`
TGTS = basic-tutorial-5

all : $(TGTS)

$(TGTS) : $(OBJS)
	$(CC) -o $(TGTS) $(OBJS) $(LFLAGS)

.c.o :
	$(CC) $(CFLAGS) -c $<

clean :
	$(RM) $(TGTS) $(OBJS)
