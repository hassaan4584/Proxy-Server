CFLAGS = -g -Wall
LINK_FLAGS = -lm
NAME = ps

ALL_SOURCES = \
$(NAME).c \
queue.c \

ALL_OBJECTS = \
$(NAME).o \
queue.o \

all: $(ALL_OBJECTS)
	$(CC) $(CFLAGS) $(ALL_OBJECTS) -o $(NAME) $(LINK_FLAGS)

clean:
	rm -rf $(ALL_OBJECTS) $(NAME)
