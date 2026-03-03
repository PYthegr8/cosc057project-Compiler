/*
 *  File Name: ir_builder.cpp
 *  Description: Builds LLVM IR from a renamed AST using the LLVM Builder algorithm.
 *  Author: Papa Yaw Owusu Nti
 */

#include "ir_builder.h"

#include <cassert>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <llvm-c/Core.h>

/* Per-function map: unique variable name -> alloca instruction. */
static std::unordered_map<std::string, LLVMValueRef> var_map;

/* Return slot alloca for current function. */
static LLVMValueRef ret_ref = nullptr;

/* Return basic block for current function. */
static LLVMBasicBlockRef retBB = nullptr;

/* References to extern functions. */
static LLVMValueRef printFn = nullptr;
static LLVMValueRef readFn  = nullptr;

/* Return the LLVM i32 type. */
static LLVMTypeRef i32Ty() {
    return LLVMInt32Type();
}

/* Return the LLVM void type. */
static LLVMTypeRef voidTy() {
    return LLVMVoidType();
}

/* Create a constant i32 value. */
static LLVMValueRef constI32(int v) {
    return LLVMConstInt(i32Ty(), (unsigned long long)v, /*SignExtend*/ 1);
}

/* Add an unconditional branch only if the block has no terminator yet. */
static void brIfNoTerminator(LLVMBuilderRef builder, LLVMBasicBlockRef fromBB, LLVMBasicBlockRef destBB) {
    LLVMValueRef term = LLVMGetBasicBlockTerminator(fromBB);
    if (term == nullptr) {
        LLVMPositionBuilderAtEnd(builder, fromBB);
        LLVMBuildBr(builder, destBB);
    }
}

/* Collect all declared variable names in a statement subtree. */
static void collectDeclNames(astNode *stmtNode, std::unordered_set<std::string> &names);

/* Generate IR for an expression and return an LLVMValueRef. */
static LLVMValueRef genIRExpr(astNode *expr, LLVMBuilderRef builder);

/* Generate IR for a statement subtree and return the ending basic block. */
static LLVMBasicBlockRef genIRStmt(astNode *stmt, LLVMBuilderRef builder, LLVMBasicBlockRef startBB, LLVMValueRef fn);

/* Delete basic blocks that have no path from entryBB (BFS reachability). */
static void removeUnreachableBlocks(LLVMValueRef fn, LLVMBasicBlockRef entryBB);

/* Declare extern functions print and read. */
static void declareExterns(LLVMModuleRef M) {
    // declare void @print(i32)
    LLVMTypeRef printArgs[1] = { i32Ty() };
    LLVMTypeRef printTy = LLVMFunctionType(voidTy(), printArgs, 1, /*isVarArg*/ 0);
    printFn = LLVMAddFunction(M, "print", printTy);

    // declare i32 @read()
    LLVMTypeRef readTy = LLVMFunctionType(i32Ty(), nullptr, 0, /*isVarArg*/ 0);
    readFn = LLVMAddFunction(M, "read", readTy);
}

/* Collect declaration names inside a block statement list. */
static void collectDeclNamesInBlock(astNode *blockStmtNode, std::unordered_set<std::string> &names) {
    if (!blockStmtNode) return;

    if (blockStmtNode->type != ast_stmt || blockStmtNode->stmt.type != ast_block) {
        collectDeclNames(blockStmtNode, names);
        return;
    }

    std::vector<astNode*> *list = blockStmtNode->stmt.block.stmt_list;
    if (!list) return;

    for (size_t i = 0; i < list->size(); ++i) {
        collectDeclNames((*list)[i], names);
    }
}

/* Collect all decl names in any statement subtree. */
static void collectDeclNames(astNode *stmtNode, std::unordered_set<std::string> &names) {
    if (!stmtNode) return;

    // Expression-statements show up as raw expression nodes
    if (stmtNode->type != ast_stmt) {
        // No declarations exist inside expressions in this grammar
        return;
    }

    switch (stmtNode->stmt.type) {
        case ast_decl:
            if (stmtNode->stmt.decl.name) {
                names.insert(std::string(stmtNode->stmt.decl.name));
            }
            break;

        case ast_block:
            collectDeclNamesInBlock(stmtNode, names);
            break;

        case ast_if:
            collectDeclNames(stmtNode->stmt.ifn.if_body, names);
            if (stmtNode->stmt.ifn.else_body) {
                collectDeclNames(stmtNode->stmt.ifn.else_body, names);
            }
            break;

        case ast_while:
            collectDeclNames(stmtNode->stmt.whilen.body, names);
            break;

        case ast_asgn:
        case ast_call:
        case ast_ret:
        default:
            break;
    }
}

/* Map relational operator to LLVMIntPredicate. */
static LLVMIntPredicate mapRelOp(rop_type op) {
    switch (op) {
        case lt:  return LLVMIntSLT;
        case gt:  return LLVMIntSGT;
        case le:  return LLVMIntSLE;
        case ge:  return LLVMIntSGE;
        case eq:  return LLVMIntEQ;
        case neq: return LLVMIntNE;
        default:  return LLVMIntNE;
    }
}

/* Generate LLVM IR for an expression node. */
static LLVMValueRef genIRExpr(astNode *expr, LLVMBuilderRef builder) {
    if (!expr) return constI32(0);

    // Generate IR based on expression node kind
    switch (expr->type) {
        case ast_cnst:
            return constI32(expr->cnst.value);

        case ast_var: {
            std::string name = expr->var.name ? std::string(expr->var.name) : std::string("");
            LLVMValueRef allocaRef = var_map[name];
            return LLVMBuildLoad2(builder, i32Ty(), allocaRef, "loadtmp");
        }

        case ast_uexpr: {
            // Unary minus: 0 - expr
            LLVMValueRef val = genIRExpr(expr->uexpr.expr, builder);
            return LLVMBuildSub(builder, constI32(0), val, "negtmp");
        }

        case ast_bexpr: {
            LLVMValueRef lhs = genIRExpr(expr->bexpr.lhs, builder);
            LLVMValueRef rhs = genIRExpr(expr->bexpr.rhs, builder);

            switch (expr->bexpr.op) {
                case add:    return LLVMBuildAdd(builder, lhs, rhs, "addtmp");
                case sub:    return LLVMBuildSub(builder, lhs, rhs, "subtmp");
                case mul:    return LLVMBuildMul(builder, lhs, rhs, "multmp");
                case divide: return LLVMBuildSDiv(builder, lhs, rhs, "divtmp");
                default:     return LLVMBuildAdd(builder, lhs, rhs, "addtmp");
            }
        }

        case ast_rexpr: {
            LLVMValueRef lhs = genIRExpr(expr->rexpr.lhs, builder);
            LLVMValueRef rhs = genIRExpr(expr->rexpr.rhs, builder);
            LLVMIntPredicate pred = mapRelOp(expr->rexpr.op);
            return LLVMBuildICmp(builder, pred, lhs, rhs, "cmptmp");
        }

        case ast_stmt: {
            // read() appears in expressions as a call node (created by createCall("read", NULL))
            if (expr->stmt.type == ast_call && expr->stmt.call.name && std::string(expr->stmt.call.name) == "read") {
                LLVMTypeRef readTy = LLVMFunctionType(i32Ty(), nullptr, 0, 0);
                return LLVMBuildCall2(builder, readTy, readFn, nullptr, 0, "readtmp");
            }

            // Other statement nodes should not appear as expressions in this grammar
            return constI32(0);
        }

        default:
            return constI32(0);
    }
}

/* Generate LLVM IR for a statement subtree. */
static LLVMBasicBlockRef genIRStmt(astNode *stmt, LLVMBuilderRef builder, LLVMBasicBlockRef startBB, LLVMValueRef fn) {
    if (!stmt) return startBB;

    // Expression-statement: evaluate and discard
    if (stmt->type != ast_stmt) {
        LLVMPositionBuilderAtEnd(builder, startBB);
        (void)genIRExpr(stmt, builder);
        return startBB;
    }

    // Generate IR based on statement kind
    switch (stmt->stmt.type) {
        case ast_asgn: {
            // Assignment: store RHS into LHS alloca
            LLVMPositionBuilderAtEnd(builder, startBB);

            astNode *lhsNode = stmt->stmt.asgn.lhs;
            astNode *rhsNode = stmt->stmt.asgn.rhs;

            LLVMValueRef rhsVal = genIRExpr(rhsNode, builder);

            // LHS is always a var node in this grammar
            std::string lhsName = lhsNode->var.name ? std::string(lhsNode->var.name) : std::string("");
            LLVMValueRef lhsAlloca = var_map[lhsName];

            LLVMBuildStore(builder, rhsVal, lhsAlloca);
            return startBB;
        }

        case ast_call: {
            // print(expr): call extern print with one i32 arg
            LLVMPositionBuilderAtEnd(builder, startBB);

            LLVMValueRef argVal = constI32(0);
            if (stmt->stmt.call.param) {
                argVal = genIRExpr(stmt->stmt.call.param, builder);
            }

            LLVMValueRef args[1] = { argVal };
            LLVMTypeRef printArgs[1] = { i32Ty() };
            LLVMTypeRef printTy = LLVMFunctionType(voidTy(), printArgs, 1, 0);
            LLVMBuildCall2(builder, printTy, printFn, args, 1, "");

            return startBB;
        }

        case ast_while: {
            // While loop: startBB -> condBB -> (trueBB | falseBB)
            LLVMPositionBuilderAtEnd(builder, startBB);

            LLVMBasicBlockRef condBB  = LLVMAppendBasicBlock(fn, "while.cond");
            LLVMBasicBlockRef trueBB  = LLVMAppendBasicBlock(fn, "while.body");
            LLVMBasicBlockRef falseBB = LLVMAppendBasicBlock(fn, "while.end");

            LLVMBuildBr(builder, condBB);

            // Condition block
            LLVMPositionBuilderAtEnd(builder, condBB);
            LLVMValueRef condVal = genIRExpr(stmt->stmt.whilen.cond, builder);
            LLVMBuildCondBr(builder, condVal, trueBB, falseBB);

            // Body block
            LLVMBasicBlockRef trueExitBB = genIRStmt(stmt->stmt.whilen.body, builder, trueBB, fn);
            brIfNoTerminator(builder, trueExitBB, condBB);

            return falseBB;
        }

        case ast_if: {
            // If / if-else: branch to trueBB or falseBB
            LLVMPositionBuilderAtEnd(builder, startBB);

            LLVMBasicBlockRef trueBB  = LLVMAppendBasicBlock(fn, "if.then");
            LLVMBasicBlockRef falseBB = LLVMAppendBasicBlock(fn, "if.else_or_end");

            LLVMValueRef condVal = genIRExpr(stmt->stmt.ifn.cond, builder);
            LLVMBuildCondBr(builder, condVal, trueBB, falseBB);

            if (stmt->stmt.ifn.else_body == nullptr) {
                // If-only: trueBB falls through to falseBB
                LLVMBasicBlockRef ifExitBB = genIRStmt(stmt->stmt.ifn.if_body, builder, trueBB, fn);
                brIfNoTerminator(builder, ifExitBB, falseBB);
                return falseBB;
            } else {
                // If-else: both sides branch to endBB
                LLVMBasicBlockRef elseStartBB = falseBB;
                LLVMBasicBlockRef endBB = LLVMAppendBasicBlock(fn, "if.end");

                LLVMBasicBlockRef ifExitBB   = genIRStmt(stmt->stmt.ifn.if_body, builder, trueBB, fn);
                LLVMBasicBlockRef elseExitBB = genIRStmt(stmt->stmt.ifn.else_body, builder, elseStartBB, fn);

                brIfNoTerminator(builder, ifExitBB, endBB);
                brIfNoTerminator(builder, elseExitBB, endBB);

                return endBB;
            }
        }

        case ast_ret: {
            // Return: store into ret_ref and branch to retBB
            LLVMPositionBuilderAtEnd(builder, startBB);

            LLVMValueRef retVal = genIRExpr(stmt->stmt.ret.expr, builder);
            LLVMBuildStore(builder, retVal, ret_ref);
            LLVMBuildBr(builder, retBB);

            // New block returned so later statements can still be connected
            LLVMBasicBlockRef endBB = LLVMAppendBasicBlock(fn, "after.ret");
            return endBB;
        }

        case ast_block: {
            // Block: connect statement list in order
            LLVMBasicBlockRef prevBB = startBB;

            std::vector<astNode*> *list = stmt->stmt.block.stmt_list;
            if (!list || list->empty()) {
                return prevBB;
            }

            for (size_t i = 0; i < list->size(); ++i) {
                prevBB = genIRStmt((*list)[i], builder, prevBB, fn);
            }

            return prevBB;
        }

        case ast_decl:
            // Declarations do not emit IR here (allocas were already made in entry)
            return startBB;

        default:
            return startBB;
    }
}

/* Delete basic blocks that have no predecessor path from entryBB. */
static void removeUnreachableBlocks(LLVMValueRef fn, LLVMBasicBlockRef entryBB) {
    std::unordered_set<LLVMBasicBlockRef> reachable;
    std::vector<LLVMBasicBlockRef> work;

    reachable.insert(entryBB);
    work.push_back(entryBB);

    // BFS over successors using terminators
    while (!work.empty()) {
        LLVMBasicBlockRef bb = work.back();
        work.pop_back();

        LLVMValueRef term = LLVMGetBasicBlockTerminator(bb);
        if (!term) continue;

        unsigned n = LLVMGetNumSuccessors(term);
        for (unsigned i = 0; i < n; ++i) {
            LLVMBasicBlockRef succ = LLVMGetSuccessor(term, i);
            if (succ && reachable.find(succ) == reachable.end()) {
                reachable.insert(succ);
                work.push_back(succ);
            }
        }
    }

    // Collect all blocks, then delete unreachable ones
    std::vector<LLVMBasicBlockRef> allBlocks;
    for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(fn); bb != nullptr; bb = LLVMGetNextBasicBlock(bb)) {
        allBlocks.push_back(bb);
    }

    for (size_t i = 0; i < allBlocks.size(); ++i) {
        LLVMBasicBlockRef bb = allBlocks[i];
        if (reachable.find(bb) == reachable.end()) {
            LLVMDeleteBasicBlock(bb);
        }
    }
}

/* Build LLVM module for the whole program AST. */
LLVMModuleRef BuildLLVMModule(astNode *root) {
    if (!root) return nullptr;

    // Create module and set the target architecture
    LLVMModuleRef M = LLVMModuleCreateWithName("minic_module");
    LLVMSetTarget(M, "x86_64-pc-linux-gnu");

    // Add extern declarations for print and read
    declareExterns(M);

    // Program contains one function node at prog.func
    if (root->type != ast_prog || root->prog.func == nullptr || root->prog.func->type != ast_func) {
        return M;
    }

    astNode *fnNode = root->prog.func;

    // Create LLVM function type from AST parameter
    unsigned paramCount = (fnNode->func.param != nullptr) ? 1 : 0;
    LLVMTypeRef paramTypes[1] = { i32Ty() };
    LLVMTypeRef fnTy = LLVMFunctionType(i32Ty(), (paramCount ? paramTypes : nullptr), paramCount, 0);

    // Add the function to the module
    LLVMValueRef fn = LLVMAddFunction(M, fnNode->func.name, fnTy);

    // Create builder and entry block
    LLVMBuilderRef builder = LLVMCreateBuilder();
    LLVMBasicBlockRef entryBB = LLVMAppendBasicBlock(fn, "entry");
    LLVMPositionBuilderAtEnd(builder, entryBB);

    // Create a set of names for parameter and local variables
    std::unordered_set<std::string> names;

    if (fnNode->func.param && fnNode->func.param->type == ast_var && fnNode->func.param->var.name) {
        names.insert(std::string(fnNode->func.param->var.name));
    }

    // Collect declared locals from the function body
    collectDeclNamesInBlock(fnNode->func.body, names);

    // Initialize var_map for this function
    var_map.clear();

    // Create allocas for all parameters and locals in entry block
    for (const std::string &name : names) {
        LLVMValueRef a = LLVMBuildAlloca(builder, i32Ty(), name.c_str());
        LLVMSetAlignment(a, 4);
        var_map[name] = a;
    }

    // Create alloca for the return value slot
    ret_ref = LLVMBuildAlloca(builder, i32Ty(), "ret");
    LLVMSetAlignment(ret_ref, 4);

    // Store function parameter into its alloca slot
    if (paramCount == 1 && fnNode->func.param && fnNode->func.param->type == ast_var) {
        LLVMValueRef param0 = LLVMGetParam(fn, 0);
        std::string pName = fnNode->func.param->var.name ? std::string(fnNode->func.param->var.name) : std::string("");
        LLVMValueRef pAlloca = var_map[pName];
        LLVMBuildStore(builder, param0, pAlloca);
    }

    // Create return basic block
    retBB = LLVMAppendBasicBlock(fn, "return");

    // Add load+ret in return block
    LLVMPositionBuilderAtEnd(builder, retBB);
    LLVMValueRef loadedRet = LLVMBuildLoad2(builder, i32Ty(), ret_ref, "retload");
    LLVMBuildRet(builder, loadedRet);

    // Generate IR for the function body
    LLVMBasicBlockRef exitBB = genIRStmt(fnNode->func.body, builder, entryBB, fn);

    // If exitBB has no terminator, branch to retBB
    LLVMValueRef exitTerm = LLVMGetBasicBlockTerminator(exitBB);
    if (exitTerm == nullptr) {
        LLVMPositionBuilderAtEnd(builder, exitBB);
        LLVMBuildBr(builder, retBB);
    }

    // Remove blocks not reachable from entry
    removeUnreachableBlocks(fn, entryBB);

    // Cleanup per-function state
    LLVMDisposeBuilder(builder);
    var_map.clear();
    ret_ref = nullptr;
    retBB = nullptr;

    return M;
}