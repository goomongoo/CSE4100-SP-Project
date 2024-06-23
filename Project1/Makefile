CC = gcc

TARGET = testlib

SRCS = list.c \
       hash.c \
       bitmap.c \
       debug.c \
       hex_dump.c \
       main.c

OBJS = $(SRCS:.c=.o)

.PHONY : clean fclean re

$(TARGET) : $(OBJS)
	$(CC) -o $(TARGET) $(OBJS)

%.o : %.c %.h debug.c debug.h
	$(CC) -c $<

bitmap.c : limits.h round.h hex_dump.h

hex_dump.c : round.h

clean :
	rm -rf $(OBJS)

fclean : clean
	rm -rf $(TARGET)

re : fclean $(TARGET)
