CC = gcc
CFLAGS = -I ../../hal/host -I ../../
LIB_DIR = ../../lib/
POSIX_DIR = ../../posix/

#printf.o bsearch.o rand.o strtoul.o rb.o list.o cbuffer.o strutil.o idtree.o 
test: test.o posix.o unix.o fdpass.o inet.o fff.h ../../lib/rb.h

NAMES = printf bsearch rand strtoul rb list cbuffer strutil idtree
PATHS = $(addprefix $(LIB_DIR), $(NAMES))
SOURCES = $(PATHS:=.c)
OBJECTS = $(NAMES:=.o)
HEADERS = $(PATHS:=.h)

$(info $(OBJECTS) ----- $(SOURCES)---)
# $(OBJECTS): $(SOURCES) $(HEADERS)
# 	$(CC) $(CFLAGS) -c $< -o $@

# $(OBJECTS): $(wildcard $(paths)/%.h) $(wildcard $(PATHS)/%.h)
# 	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o test

# printf.o: $(LIB_DIR)/printf.c $(LIB_DIR)/printf.h
# 	$(CC) $(CFLAGS) -c $< -o $@
# bsearch.o: $(LIB_DIR)/bsearch.c $(LIB_DIR)/bsearch.h
# 	$(CC) $(CFLAGS) -c $< -o $@
# rand.o: $(LIB_DIR)/rand.c $(LIB_DIR)/rand.h
# 	$(CC) $(CFLAGS) -c $< -o $@
# strtoul.o: $(LIB_DIR)/strtoul.c $(LIB_DIR)/strtoul.h
# 	$(CC) $(CFLAGS) -c $< -o $@
# rb.o: $(LIB_DIR)/rb.c $(LIB_DIR)/rb.h
# 	$(CC) $(CFLAGS) -c $< -o $@
# list.o: $(LIB_DIR)/list.c $(LIB_DIR)/list.h
# 	$(CC) $(CFLAGS) -c $< -o $@
# cbuffer.o: $(LIB_DIR)/cbuffer.c $(LIB_DIR)/cbuffer.h
# 	$(CC) $(CFLAGS) -c $< -o $@
# strutil.o: $(LIB_DIR)/strutil.c $(LIB_DIR)/strutil.h
# 	$(CC) $(CFLAGS) -c $< -o $@
# idtree.o: $(LIB_DIR)/idtree.c $(LIB_DIR)/idtree.h
# 	$(CC) $(CFLAGS) -c $< -o $@

posix.o: $(POSIX_DIR)posix.c $(POSIX_DIR)posix.h
	$(CC) $(CFLAGS) -c $< -o $@

unix.o: $(POSIX_DIR)unix.c $(POSIX_DIR)posix_private.h
	$(CC) $(CFLAGS) -c $< -o $@

fdpass.o: $(POSIX_DIR)fdpass.c $(POSIX_DIR)fdpass.h $(POSIX_DIR)posix_private.h
	$(CC) $(CFLAGS) -c $< -o $@

inet.o: $(POSIX_DIR)inet.c $(POSIX_DIR)posix_private.h
	$(CC) $(CFLAGS) -c $< -o $@



# $(CC) $(CFLAGS) -c $< -o $@
# main.o: main.c helper.h
#     $(CC) $(CFLAGS) -c $< -o $@

# helper.o: helper.c helper.h
#     $(CC) $(CFLAGS) -c $< -o $@