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

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>

extern FILE *yyin;
extern int yyparse(void);
extern int yylex_destroy(void);

// AST root defined in parser
extern astNode *root;

/* Print simple usage message */
static void printUsage() {
    printf("Usage: ./compiler <input_file>\n");
}

/* Entry point */
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

    // Run parser
    if (yyparse() != 0 || root == NULL) {
        printf("Parsing failed.\n");
        fclose(yyin);
        return 1;
    }

    // Run semantic analysis
    if (!runSemanticAnalysis(root)) {
        printf("Semantic analysis failed.\n");
        fclose(yyin);
        return 1;
    }

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

    // Cleanup
    LLVMDisposeModule(module);
    LLVMShutdown();
    freeNode(root);
    yylex_destroy();
    fclose(yyin);

    printf("Compilation successful. Output written to output.ll\n");
    return 0;
}