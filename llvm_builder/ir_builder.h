/*
*  File Name: ir_builder.h
 *  Description: Builds an LLVM module from a renamed miniC AST.
 *  Author: Papa Yaw Owusu Nti
 */

#ifndef IR_BUILDER_H
#define IR_BUILDER_H

#include "ast.h"
#include <llvm-c/Core.h>

/*
 * Builds LLVM IR for the whole program AST and returns the LLVM module.
 */
LLVMModuleRef BuildLLVMModule(astNode *root);

#endif