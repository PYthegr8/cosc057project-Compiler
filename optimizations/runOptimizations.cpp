/*
* runOptimizations.cpp
 *
 * Loads an LLVM IR file, runs local optimizations on each function,
 * and prints the optimized IR.
 */

#include <cstdio>
#include <cstdlib>

#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Support.h>

extern bool runLocalOptimizations(LLVMValueRef function);

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <input.ll>\n", argv[0]);
        return 1;
    }

    const char* inputFile = argv[1];

    LLVMContextRef context = LLVMContextCreate();
    LLVMMemoryBufferRef memoryBuffer = nullptr;
    LLVMModuleRef module = nullptr;
    char* errorMessage = nullptr;

    // Load file into memory buffer
    if (LLVMCreateMemoryBufferWithContentsOfFile(inputFile, &memoryBuffer, &errorMessage) != 0) {
        std::fprintf(stderr, "Error reading file: %s\n", errorMessage);
        return 1;
    }

    // Parse IR into module
    if (LLVMParseIRInContext(context, memoryBuffer, &module, &errorMessage) != 0) {
        std::fprintf(stderr, "Error parsing IR: %s\n", errorMessage);
        return 1;
    }

    // Run local optimizations on each function
    for (LLVMValueRef function = LLVMGetFirstFunction(module);
         function != nullptr;
         function = LLVMGetNextFunction(function)) {

        if (LLVMCountBasicBlocks(function) == 0) continue;

        // Global fixpoint: constant propagation followed by constant folding
        while (true) {
            bool changed = false;

            changed |= constantPropagation(function);
            changed |= constantFolding(function);
            changed |= deadCodeElimination(function);

            if (!changed) break;
        }

        // Local cleanup
        commonSubexpressionElimination(function);
        deadCodeElimination(function);
         }

    // Print optimized module
    char* output = LLVMPrintModuleToString(module);
    std::printf("%s", output);
    LLVMDisposeMessage(output);

    LLVMDisposeModule(module);
    LLVMContextDispose(context);

    return 0;
}
