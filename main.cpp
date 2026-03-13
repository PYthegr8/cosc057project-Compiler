/*
 *  File Name: main.cpp
 *  Description: Driver for the miniC compiler pipeline.
 *  Author: Papa Yaw Owusu Nti
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ast.h"
#include "parsing/semantic.h"
#include "parsing/preprocessor.h"
#include "llvm_builder/ir_builder.h"
#include "compiler_backend/register_allocator.h"
#include "compiler_backend/assembly_gen.h"

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <stdio.h>

extern FILE *yyin;
extern int yyparse(void);
extern int yylex_destroy(void);

// AST root defined in parser
extern astNode *root;

/* Print simple usage message */
static void printUsage() {
    printf("Usage: ./compiler <input_file>\n");
}

int main(int argc, char **argv) {

    if (argc != 2) {
        printUsage();
        return 1;
    }

    const char *filename = argv[1];
    yyin = fopen(filename, "r");

    if (!yyin) {
        printf("Error: cannot open file %s\n", filename);
        return 1;
    }

    printf("File parsing started\n");

    // Run parser
    if (yyparse() != 0 || root == NULL) {
        printf("Parsing failed.\n");
        fclose(yyin);
        return 1;
    }

    printf("Parsing successful\n");

    // Run semantic analysis
    if (SemanticAnalysis(root) != 0) {
        printf("Semantic analysis failed.\n");
        fclose(yyin);
        return 1;
    }

    printf("Semantic analysis successful\n");

    // Run variable renaming pass
    RenameVariablesUnique(root);

    // Build LLVM IR
    LLVMModuleRef module = BuildLLVMModule(root);
    if (!module) {
        printf("IR builder failed.\n");
        fclose(yyin);
        return 1;
    }

    // Verify module
    char *error = NULL;
    if (LLVMVerifyModule(module, LLVMAbortProcessAction, &error)) {
        printf("LLVM verification failed:\n%s\n", error);
        LLVMDisposeMessage(error);
        LLVMDisposeModule(module);
        fclose(yyin);
        return 1;
    }

    // Write LLVM IR to file
    if (LLVMPrintModuleToFile(module, "output.ll", &error) != 0) {
        printf("Error writing LLVM IR:\n%s\n", error);
        LLVMDisposeMessage(error);
    }

    // Perform register allocation for all functions
    printf("Performing register allocation\n");
    RegisterMap reg_map;
    
    // Process each function in the module
    for (LLVMValueRef func = LLVMGetFirstFunction(module);
         func != NULL;
         func = LLVMGetNextFunction(func)) {
        
        // Skip function declarations
        if (LLVMIsDeclaration(func)) {
            continue;
        }
        
        // Allocate registers for this function
        RegisterMap func_reg_map = allocate_registers(func);
        
        // Merge into overall register map
        for (auto& pair : func_reg_map) {
            reg_map[pair.first] = pair.second;
        }
    }
    printf("Register allocation completed\n");

    // Generate assembly code
    printf("Generating assembly code...\n");
    if (generateAssembly(module, reg_map, "output.s") != 0) {
        printf("Assembly generation failed\n");
        LLVMDisposeModule(module);
        fclose(yyin);
        return 1;
    }
    printf("Assembly generation completed. Output written to output.s\n");

    // Cleanup
    LLVMDisposeModule(module);
    LLVMShutdown();
    freeNode(root);
    if (yyin) fclose(yyin);
    yylex_destroy();

    printf("Compilation successful. Output written to output.ll and output.s\n");
    return 0;
}