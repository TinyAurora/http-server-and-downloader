all: http_downloader.out myhttpd_terminal_version.out myhttpd_browser_version.out
http_downloader.out: http_downloader.c
	gcc -o $@ $^
myhttpd_terminal_version.out: myhttpd_terminal_version.c
	gcc -o $@ $^
myhttpd_browser_version.out: myhttpd_browser_version.c
	gcc -o $@ $^
clean:
	rm -rf *.o *.out
.PHONY: clean
