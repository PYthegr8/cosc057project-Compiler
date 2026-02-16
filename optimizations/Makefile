LLVMCODE = optimizer
IN = test.ll
OUT = test_opt.ll

$(LLVMCODE): runOptimizations.cpp localOptimizations.cpp globalOptimizations.cpp
	clang++ -g `llvm-config-17 --cxxflags --ldflags --libs core irreader support` \
	runOptimizations.cpp localOptimizations.cpp globalOptimizations.cpp -o $(LLVMCODE)

run: $(LLVMCODE)
	./$(LLVMCODE) $(IN) > $(OUT)

clean:
	rm -rf $(LLVMCODE)
	rm -rf *.o
	rm -rf *.out
	rm -rf *.txt
	rm -rf $(OUT)
