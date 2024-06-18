all: compile run

compile:
	gcc -o myServer server.c sv_cl_lib.h
	gcc -o myClient client.c sv_cl_lib.h

run:

clean:
	rm -f myServer myClient
