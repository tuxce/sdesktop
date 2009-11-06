sdesktop: sdesktop.c
	gcc sdesktop.c -o sdesktop -lX11

install: sdesktop
	install -m 755 sdesktop $(DESTDIR)/usr/bin

all: sdesktop

clean:
	rm sdesktop

uninstall:
	rm $(DESTDIR)/usr/bin/sdesktop

