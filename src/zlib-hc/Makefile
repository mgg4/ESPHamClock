CXX=g++

LIBOBJ =  \
	adler32.o \
	crc32.o \
	deflate.o \
	infback.o \
	inffast.o \
	inflate.o \
	inftrees.o \
	trees.o \
	zutil.o

libzlib-hc.a: $(LIBOBJ)
	ar rc libzlib-hc.a $(LIBOBJ)
	ranlib libzlib-hc.a

zpipe: zpipe.o libzlib-hc.a
	$(CXX) -o zpipe zpipe.o libzlib-hc.a

clean:
	rm -f *.o *.a zpipe

