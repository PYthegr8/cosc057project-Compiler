/*
 * File Name: assembly_gen.h
 * Description: Header file for x86 assembly code generation from LLVM IR
 * Author: Papa Yaw Owusu Nti
 */

#ifndef ASSEMBLY_GEN_H
#define ASSEMBLY_GEN_H

#include <llvm-c/Core.h>
#include <map>
#include <unordered_map>
#include <string>
#include <cstdio>

/*
 * Map from LLVM basic blocks to their assembly labels
 */
typedef std::map<LLVMBasicBlockRef, std::string> BBLabelMap;

/*
 * Map from LLVM values to their stack offset from %ebp
 * Positive offsets for parameters, negative for locals
 */
typedef std::unordered_map<LLVMValueRef, int> OffsetMap;

/* Create unique labels for each basic block in a function */
void createBBLabels(LLVMValueRef func, BBLabelMap& bb_labels);

/* Output assembly directives and function label to file */
void printDirectives(LLVMValueRef func, FILE* output);

/* Output function epilogue (leave and ret instructions) */
void printFunctionEnd(FILE* output);

/* Calculate stack offsets for all function parameters and local variables */
void getOffsetMap(LLVMValueRef func, OffsetMap& offset_map, int& localMem);

/* Generate x86-32 assembly from LLVM IR using register allocation results */
int generateAssembly(LLVMModuleRef module,
                    std::unordered_map<LLVMValueRef, int>& reg_map,
                    const char* output_filename);

#endif
