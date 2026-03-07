LLVMCODE = compiler
OPTCODE = optimizer

IN = llvm_builder/builder_tests/p1.c
OUT = output.ll
OUT_OPT = output_opt.ll

LLVMFLAGS = `llvm-config-17 --cxxflags --ldflags --libs core analysis`

# include paths so headers like ast.h can be found from any folder
INCLUDES = -I. -Iparsing -Illvm_builder -Ifrontend -Isemantic_analysis -Ioptimizations
# sources
COMPILER_SRC = \
	main.cpp \
	ast.c \
	parsing/parsing.tab.c \
	parsing/lex.yy.c \
	$(wildcard */semantic.cpp) \
	$(wildcard */preprocessor.cpp) \
	$(wildcard */ir_builder.cpp)
OPT_SRC = \
	optimizations/runOptimizations.cpp \
	optimizations/localOptimizations.cpp \
	optimizations/globalOptimizations.cpp

# regenerate parser outputs
parsing/parsing.tab.c parsing/parsing.tab.h: parsing/parsing.y
	bison -d -o parsing/parsing.tab.c parsing/parsing.y

parsing/lex.yy.c: parsing/parse.l
	flex -o parsing/lex.yy.c parsing/parse.l

$(LLVMCODE): parsing/parsing.tab.c parsing/lex.yy.c $(COMPILER_SRC)
	clang++ -g $(INCLUDES) $(LLVMFLAGS) $(COMPILER_SRC) -o $(LLVMCODE)

$(OPTCODE): $(OPT_SRC)
	clang++ -g `llvm-config-17 --cxxflags --ldflags --libs core irreader support` \
	$(OPT_SRC) -o $(OPTCODE)

run: $(LLVMCODE)
	./$(LLVMCODE) $(IN)

opt: $(OPTCODE)
	./$(OPTCODE) $(OUT) > $(OUT_OPT)

clean:
	rm -rf $(LLVMCODE)
	rm -rf $(OPTCODE)
	rm -rf *.o
	rm -rf *.out
	rm -rf *.txt
	rm -rf $(OUT)
	rm -rf $(OUT_OPT)
	rm -rf parsing/parsing.tab.c parsing/parsing.tab.h parsing/lex.yy.c