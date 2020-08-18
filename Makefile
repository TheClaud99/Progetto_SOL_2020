CC			=  gcc
CFLAGS	    += -Wall -g
INCLUDES	= -I.
LDFLAGS 	= -L.
OPTFLAGS	= -O3 -DNDEBUG
LIBS        = -lpthread

# aggiungere qui altri targets
TARGETS		= supermercato \
		  direttore


.PHONY: all clean cleanall
.SUFFIXES: .c .h

# %.o: %.c
# 	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all: $(TARGETS)

supermercato: supermercato.c libQueue.a utils.h global.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

direttore: direttore.c libQueue.a utils.h global.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

libQueue.a: queue.o queue.h
	$(AR) $(ARFLAGS) $@ $<

test1: 
	echo "eseguo test"
	./tests/test1/test1.sh

clean: 
	rm -f $(TARGETS)
cleanall: clean
	\rm -f *.o *~ *.a
