
PROJECT := cinterion


.PHONY: all clean distclean install

all:
	make all -C $(PROJECT)
	
clean:
	make clean -C cinterion
	rm -f *.o
	
distclean:
	make distclean -C cinterion
	rm -f *.o

install:
	make install -C $(PROJECT)
