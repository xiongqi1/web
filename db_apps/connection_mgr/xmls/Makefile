#
# auto apn xml file generator
#

ifeq ($(PLATFORM),Platypus)
	INSTALL_TYPE:=install_platypus
else
	INSTALL_TYPE:=install_generic
endif


.PHONY: all

all: output.xml

merge_apn: merge_apn.c
	gcc -O0 -g merge_apn.c -lmxml -lpthread -o $@

output.xml: merge_apn base.xml addon*.xml xmlgen.sh xmlindent.sh
	# generate output.xml from auto apn xml files
	./xmlgen.sh

install_script:
	install -m 0755 -d $(INSTALLDIR)/usr/bin
	install -m 0755 xmlgen.sh $(INSTALLDIR)/usr/bin/xmlgen.sh
	install -m 0755 xmlindent.sh $(INSTALLDIR)/usr/bin/xmlindent.sh

install_platypus: install_script all
	install -m 0755 -d $(INSTALLDIR)/etc_ro/www/internet
	install -m 0644 output.xml $(INSTALLDIR)/etc_ro/www/internet/apnList.xml
	
	
install_generic: install_script all
	install -m 0755 -d $(INSTALLDIR)/www/cgi-bin
	install -m 0644 output.xml $(INSTALLDIR)/www/cgi-bin/apnList.xml
	ln -sf cgi-bin/apnList.xml $(INSTALLDIR)/www/apnList.xml

install: $(INSTALL_TYPE)
	# done
	
distclean clean:
	rm -f *.o output.xml merge_apn
