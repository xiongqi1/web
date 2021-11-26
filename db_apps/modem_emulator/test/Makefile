# set compiler environment variables
CC = gcc
LD = ld
STRIP = strip

CSD_PROJ = metering_emul
CSD_SRC = metering_emul.c term.c
CSD_OBJ = $(patsubst %.c,%.o,$(CSD_SRC))

PAD_PROJ = metering_pad_emul
PAD_SRC = metering_pad_emul.c
PAD_OBJ = $(patsubst %.c,%.o,$(PAD_SRC))

.PHONY: all
all: $(CSD_PROJ) $(PAD_PROJ)

$(CSD_PROJ): $(CSD_OBJ)
	$(CC) $(LDFLAGS) $(CSD_OBJ) $(LIBS) -o $@

$(PAD_PROJ): $(PAD_OBJ)
	$(CC) $(LDFLAGS) $(PAD_OBJ) $(LIBS) -o $@
	
.PHONY: clean distclean
clean distclean:
	rm -f $(CSD_PROJ) $(PAD_PROJ) *.o
