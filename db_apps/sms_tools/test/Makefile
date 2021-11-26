# set compiler environment variables
CC = gcc
LD = ld
STRIP = strip

PROJ = sms_client
SRC = sms_client.c
OBJ = $(patsubst %.c,%.o, $(SRC))

.PHONY: all
all: $(PROJ)

$(PROJ): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@
	
.PHONY: clean distclean
clean distclean:
	rm -f $(PROJ) *.o
