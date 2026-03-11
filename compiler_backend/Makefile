# Makefile for Register Allocator and Assembly Generator

LLVMFLAGS = `llvm-config-17 --cxxflags --ldflags --libs core analysis`

# Object files
OBJ = register_allocator.o assembly_gen.o

# Library
LIB = libregisterallocator.a

# Build the library
all: $(LIB)

$(LIB): $(OBJ)
	ar rcs $(LIB) $(OBJ)

register_allocator.o: register_allocator.cpp register_allocator.h
	clang++ -c $(LLVMFLAGS) register_allocator.cpp -o register_allocator.o

assembly_gen.o: assembly_gen.cpp assembly_gen.h register_allocator.h
	clang++ -c $(LLVMFLAGS) assembly_gen.cpp -o assembly_gen.o

clean:
	rm -f $(OBJ) $(LIB)

.PHONY: all clean
