sdesktop: sdesktop.c
	gcc $(CFLAGS) sdesktop.c -o sdesktop -lX11

install: sdesktop
	install -D -m 755 sdesktop $(DESTDIR)/usr/bin/sdesktop

all: sdesktop

clean:
	rm sdesktop

uninstall:
	rm $(DESTDIR)/usr/bin/sdesktop

