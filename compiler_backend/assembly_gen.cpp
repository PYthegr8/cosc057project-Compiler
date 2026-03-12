/*
 * File Name: assembly_gen.cpp
 * Description: Implementation of x86 assembly code generation from LLVM IR
 * Author: Papa Yaw Owusu Nti
 */

#include "assembly_gen.h"
#include "register_allocator.h"
#include <cassert>
#include <iostream>
#include <sstream>
#include <cstring>

/*
 * Helper function to get register name from register id
 */
static const char* getRegisterName(int reg_id) {
    switch (reg_id) {
        case REG_EBX: return "%ebx";
        case REG_ECX: return "%ecx";
        case REG_EDX: return "%edx";
        default: return NULL;
    }
}

/*
 * Helper function to get jump instruction from comparison predicate
 */
static const char* getJumpName(LLVMIntPredicate pred) {
    switch (pred) {
        case LLVMIntSLT: return "jl";
        case LLVMIntSGT: return "jg";
        case LLVMIntSLE: return "jle";
        case LLVMIntSGE: return "jge";
        case LLVMIntEQ:  return "je";
        case LLVMIntNE:  return "jne";
        default: return NULL;
    }
}

/*
 * Helper function to get set instruction from comparison predicate
 */
static const char* getSetName(LLVMIntPredicate pred) {
    switch (pred) {
        case LLVMIntSLT: return "setl";
        case LLVMIntSGT: return "setg";
        case LLVMIntSLE: return "setle";
        case LLVMIntSGE: return "setge";
        case LLVMIntEQ:  return "sete";
        case LLVMIntNE:  return "setne";
        default: return NULL;
    }
}

/*
 * Helper function to emit an assembly instruction
 */
static void emit(FILE* output, const char* instr) {
    fprintf(output, "\t%s\n", instr);
}

/*
 * Helper function to emit an assembly instruction with operands
 */
static void emitOp(FILE* output, const char* opcode, const char* op1, const char* op2 = NULL) {
    if (op2) {
        fprintf(output, "\t%s %s, %s\n", opcode, op1, op2);
    } else {
        fprintf(output, "\t%s %s\n", opcode, op1);
    }
}

/*
 * Create labels for all basic blocks in a function
 */
void createBBLabels(LLVMValueRef func, BBLabelMap& bb_labels) {
    bb_labels.clear();

    int label_count = 0;
    for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(func);
         bb != NULL;
         bb = LLVMGetNextBasicBlock(bb)) {

        std::stringstream ss;
        ss << ".LBB" << label_count;
        bb_labels[bb] = ss.str();
        label_count++;
    }
}

/*
 * Print assembly directives for a function
 */
void printDirectives(LLVMValueRef func, FILE* output) {
    const char* func_name = LLVMGetValueName(func);

    fprintf(output, "\t.text\n");
    fprintf(output, "\t.globl %s\n", func_name);
    fprintf(output, "\t.type %s, @function\n", func_name);
    fprintf(output, "%s:\n", func_name);
}

/*
 * Print function epilogue
 */
void printFunctionEnd(FILE* output) {
    emit(output, "leave");
    emit(output, "ret");
}

/*
 * Compute stack offsets for all values
 */
void getOffsetMap(LLVMValueRef func, OffsetMap& offset_map, int& localMem) {
    offset_map.clear();
    localMem = 4;

    // Handle function parameters
    int param_count = LLVMCountParams(func);
    for (int i = 0; i < param_count; i++) {
        LLVMValueRef param = LLVMGetParam(func, i);
        offset_map[param] = 8 + i * 4;
    }

    // Process each basic block
    for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(func);
         bb != NULL;
         bb = LLVMGetNextBasicBlock(bb)) {

        // Process each instruction
        for (LLVMValueRef instr = LLVMGetFirstInstruction(bb);
             instr != NULL;
             instr = LLVMGetNextInstruction(instr)) {

            LLVMOpcode opcode = LLVMGetInstructionOpcode(instr);

            if (opcode == LLVMAlloca) {
                // Allocate space for local variable
                localMem += 4;
                offset_map[instr] = -localMem;
            }
            else if (opcode == LLVMStore) {
                LLVMValueRef val = LLVMGetOperand(instr, 0);
                LLVMValueRef ptr = LLVMGetOperand(instr, 1);

                // Check if storing to a parameter
                bool is_param = false;
                for (int i = 0; i < param_count; i++) {
                    LLVMValueRef param = LLVMGetParam(func, i);
                    if (val == param) {
                        is_param = true;
                        // Copy parameter's offset to the pointer
                        if (offset_map.find(ptr) != offset_map.end()) {
                            int param_offset = offset_map[param];
                            offset_map[ptr] = param_offset;
                        }
                        break;
                    }
                }

                if (!is_param && !LLVMIsConstant(val)) {
                    // Store temporary to memory location
                    if (offset_map.find(ptr) != offset_map.end()) {
                        int ptr_offset = offset_map[ptr];
                        offset_map[val] = ptr_offset;
                    }
                }
            }
            else if (opcode == LLVMLoad) {
                LLVMValueRef ptr = LLVMGetOperand(instr, 0);
                if (offset_map.find(ptr) != offset_map.end()) {
                    int ptr_offset = offset_map[ptr];
                    offset_map[instr] = ptr_offset;
                }
            }
        }
    }
}

/*
 * Generate assembly for a return instruction
 */
static void generateReturn(LLVMValueRef instr,
                          std::unordered_map<LLVMValueRef, int>& reg_map,
                          OffsetMap& offset_map,
                          FILE* output) {

    LLVMValueRef ret_val = LLVMGetOperand(instr, 0);

    if (LLVMIsConstant(ret_val)) {
        // Return constant
        long long const_val = LLVMConstIntGetSExtValue(ret_val);
        fprintf(output, "\tmovl $%lld, %%eax\n", const_val);
    } else {
        // Return variable
        if (reg_map.find(ret_val) != reg_map.end() && reg_map[ret_val] != SPILLED) {
            // In register
            const char* reg_name = getRegisterName(reg_map[ret_val]);
            emitOp(output, "movl", reg_name, "%eax");
        } else {
            // In memory
            if (offset_map.find(ret_val) != offset_map.end()) {
                int offset = offset_map[ret_val];
                fprintf(output, "\tmovl %d(%%ebp), %%eax\n", offset);
            }
        }
    }
}

/*
 * Generate assembly for a load instruction
 */
static void generateLoad(LLVMValueRef instr,
                        std::unordered_map<LLVMValueRef, int>& reg_map,
                        OffsetMap& offset_map,
                        FILE* output) {

    LLVMValueRef ptr = LLVMGetOperand(instr, 0);

    if (reg_map.find(instr) != reg_map.end() && reg_map[instr] != SPILLED) {
        // Result goes to register
        const char* reg_name = getRegisterName(reg_map[instr]);
        if (offset_map.find(ptr) != offset_map.end()) {
            int offset = offset_map[ptr];
            fprintf(output, "\tmovl %d(%%ebp), %s\n", offset, reg_name);
        }
    }
}

/*
 * Generate assembly for a store instruction
 */
static void generateStore(LLVMValueRef instr,
                         std::unordered_map<LLVMValueRef, int>& reg_map,
                         OffsetMap& offset_map,
                         FILE* output) {

    LLVMValueRef val = LLVMGetOperand(instr, 0);
    LLVMValueRef ptr = LLVMGetOperand(instr, 1);

    // Skip if storing parameter
    int param_count = LLVMCountParams(LLVMGetBasicBlockParent(LLVMGetInstructionParent(instr)));
    bool is_param = false;
    for (int i = 0; i < param_count; i++) {
        LLVMValueRef param = LLVMGetParam(LLVMGetBasicBlockParent(LLVMGetInstructionParent(instr)), i);
        if (val == param) {
            is_param = true;
            break;
        }
    }

    if (is_param) return;

    if (offset_map.find(ptr) != offset_map.end()) {
        int offset = offset_map[ptr];

        if (LLVMIsConstant(val)) {
            // Store constant
            long long const_val = LLVMConstIntGetSExtValue(val);
            fprintf(output, "\tmovl $%lld, %d(%%ebp)\n", const_val, offset);
        } else {
            // Store variable
            if (reg_map.find(val) != reg_map.end() && reg_map[val] != SPILLED) {
                // From register
                const char* reg_name = getRegisterName(reg_map[val]);
                fprintf(output, "\tmovl %s, %d(%%ebp)\n", reg_name, offset);
            } else {
                // From memory via %eax
                if (offset_map.find(val) != offset_map.end()) {
                    int val_offset = offset_map[val];
                    fprintf(output, "\tmovl %d(%%ebp), %%eax\n", val_offset);
                    fprintf(output, "\tmovl %%eax, %d(%%ebp)\n", offset);
                }
            }
        }
    }
}

/*
 * Generate assembly for a call instruction
 */
static void generateCall(LLVMValueRef instr,
                        std::unordered_map<LLVMValueRef, int>& reg_map,
                        OffsetMap& offset_map,
                        FILE* output) {

    // Save caller-saved registers
    emitOp(output, "pushl", "%ecx");
    emitOp(output, "pushl", "%edx");

    int num_operands = LLVMGetNumOperands(instr);
    int arg_count = num_operands - 1;

    // Handle parameter if any
    if (arg_count == 1) {
        LLVMValueRef param = LLVMGetOperand(instr, 0);

        if (LLVMIsConstant(param)) {
            long long const_val = LLVMConstIntGetSExtValue(param);
            fprintf(output, "\tpushl $%lld\n", const_val);
        } else {
            if (reg_map.find(param) != reg_map.end() && reg_map[param] != SPILLED) {
                const char* reg_name = getRegisterName(reg_map[param]);
                emitOp(output, "pushl", reg_name);
            } else {
                if (offset_map.find(param) != offset_map.end()) {
                    int offset = offset_map[param];
                    fprintf(output, "\tpushl %d(%%ebp)\n", offset);
                }
            }
        }
    }

    // Extract function name from the last operand
    const char* func_name = NULL;
    if (num_operands > 0) {
        LLVMValueRef callee = LLVMGetOperand(instr, num_operands - 1);
        func_name = LLVMGetValueName(callee);
    }

    if (func_name && strlen(func_name) > 0) {
        fprintf(output, "\tcall %s\n", func_name);
    }

    // Clean up parameter if pushed
    if (arg_count == 1) {
        emitOp(output, "addl", "$4", "%esp");
    }

    // Restore registers
    emitOp(output, "popl", "%edx");
    emitOp(output, "popl", "%ecx");

    // Handle return value if call has result
    if (LLVMGetTypeKind(LLVMTypeOf(instr)) != LLVMVoidTypeKind) {
        if (reg_map.find(instr) != reg_map.end() && reg_map[instr] != SPILLED) {
            const char* reg_name = getRegisterName(reg_map[instr]);
            emitOp(output, "movl", "%eax", reg_name);
        } else {
            if (offset_map.find(instr) != offset_map.end()) {
                int offset = offset_map[instr];
                fprintf(output, "\tmovl %%eax, %d(%%ebp)\n", offset);
            }
        }
    }
}

/*
 * Generate assembly for a branch instruction
 */
static void generateBranch(LLVMValueRef instr,
                          BBLabelMap& bb_labels,
                          std::unordered_map<LLVMValueRef, int>& reg_map,
                          OffsetMap& offset_map,
                          FILE* output) {

    int num_operands = LLVMGetNumOperands(instr);

    if (num_operands == 1) {
        // Unconditional branch
        LLVMBasicBlockRef target = (LLVMBasicBlockRef)LLVMGetOperand(instr, 0);
        if (bb_labels.find(target) != bb_labels.end()) {
            fprintf(output, "\tjmp %s\n", bb_labels[target].c_str());
        }
    } else {
        // Conditional branch
        LLVMValueRef cond = LLVMGetOperand(instr, 0);
        LLVMBasicBlockRef true_bb = (LLVMBasicBlockRef)LLVMGetOperand(instr, 2);
        LLVMBasicBlockRef false_bb = (LLVMBasicBlockRef)LLVMGetOperand(instr, 1);

        if (LLVMGetInstructionOpcode(cond) == LLVMICmp) {
            LLVMIntPredicate pred = LLVMGetICmpPredicate(cond);
            const char* jump_name = getJumpName(pred);

            if (jump_name &&
                bb_labels.find(true_bb) != bb_labels.end() &&
                bb_labels.find(false_bb) != bb_labels.end()) {

                fprintf(output, "\t%s %s\n", jump_name, bb_labels[true_bb].c_str());
                fprintf(output, "\tjmp %s\n", bb_labels[false_bb].c_str());
            }
        }
    }
}

/*
 * Generate assembly for arithmetic instructions
 */
static void generateArithmetic(LLVMValueRef instr,
                              std::unordered_map<LLVMValueRef, int>& reg_map,
                              OffsetMap& offset_map,
                              FILE* output) {

    LLVMOpcode opcode = LLVMGetInstructionOpcode(instr);
    LLVMValueRef op1 = LLVMGetOperand(instr, 0);
    LLVMValueRef op2 = LLVMGetOperand(instr, 1);

    const char* op_name = NULL;
    switch (opcode) {
        case LLVMAdd: op_name = "addl"; break;
        case LLVMSub: op_name = "subl"; break;
        case LLVMMul: op_name = "imull"; break;
        default: return;
    }

    // Determine destination register
    const char* dest_reg = "%eax";
    if (reg_map.find(instr) != reg_map.end() && reg_map[instr] != SPILLED) {
        dest_reg = getRegisterName(reg_map[instr]);
    }

    // Load first operand
    if (LLVMIsConstant(op1)) {
        long long const_val = LLVMConstIntGetSExtValue(op1);
        fprintf(output, "\tmovl $%lld, %s\n", const_val, dest_reg);
    } else {
        if (reg_map.find(op1) != reg_map.end() && reg_map[op1] != SPILLED) {
            const char* src_reg = getRegisterName(reg_map[op1]);
            if (strcmp(src_reg, dest_reg) != 0) {
                emitOp(output, "movl", src_reg, dest_reg);
            }
        } else {
            if (offset_map.find(op1) != offset_map.end()) {
                int offset = offset_map[op1];
                fprintf(output, "\tmovl %d(%%ebp), %s\n", offset, dest_reg);
            }
        }
    }

    // Apply operation with second operand
    if (LLVMIsConstant(op2)) {
        long long const_val = LLVMConstIntGetSExtValue(op2);
        fprintf(output, "\t%s $%lld, %s\n", op_name, const_val, dest_reg);
    } else {
        if (reg_map.find(op2) != reg_map.end() && reg_map[op2] != SPILLED) {
            const char* src_reg = getRegisterName(reg_map[op2]);
            fprintf(output, "\t%s %s, %s\n", op_name, src_reg, dest_reg);
        } else {
            if (offset_map.find(op2) != offset_map.end()) {
                int offset = offset_map[op2];
                fprintf(output, "\t%s %d(%%ebp), %s\n", op_name, offset, dest_reg);
            }
        }
    }

    // Store result if it goes to memory
    if (reg_map.find(instr) == reg_map.end() || reg_map[instr] == SPILLED) {
        if (offset_map.find(instr) != offset_map.end()) {
            int offset = offset_map[instr];
            fprintf(output, "\tmovl %s, %d(%%ebp)\n", dest_reg, offset);
        }
    }
}

/*
 * Generate assembly for compare instructions
 */
static void generateCompare(LLVMValueRef instr,
                           std::unordered_map<LLVMValueRef, int>& reg_map,
                           OffsetMap& offset_map,
                           FILE* output) {

    LLVMValueRef op1 = LLVMGetOperand(instr, 0);
    LLVMValueRef op2 = LLVMGetOperand(instr, 1);
    LLVMIntPredicate pred = LLVMGetICmpPredicate(instr);

    const char* set_name = getSetName(pred);
    if (!set_name) return;

    // Determine destination register
    const char* dest_reg = "%eax";
    if (reg_map.find(instr) != reg_map.end() && reg_map[instr] != SPILLED) {
        dest_reg = getRegisterName(reg_map[instr]);
    }

    // Load first operand
    if (LLVMIsConstant(op1)) {
        long long const_val = LLVMConstIntGetSExtValue(op1);
        fprintf(output, "\tmovl $%lld, %s\n", const_val, dest_reg);
    } else {
        if (reg_map.find(op1) != reg_map.end() && reg_map[op1] != SPILLED) {
            const char* src_reg = getRegisterName(reg_map[op1]);
            if (strcmp(src_reg, dest_reg) != 0) {
                emitOp(output, "movl", src_reg, dest_reg);
            }
        } else {
            if (offset_map.find(op1) != offset_map.end()) {
                int offset = offset_map[op1];
                fprintf(output, "\tmovl %d(%%ebp), %s\n", offset, dest_reg);
            }
        }
    }

    // Compare with second operand
    if (LLVMIsConstant(op2)) {
        long long const_val = LLVMConstIntGetSExtValue(op2);
        fprintf(output, "\tcmpl $%lld, %s\n", const_val, dest_reg);
    } else {
        if (reg_map.find(op2) != reg_map.end() && reg_map[op2] != SPILLED) {
            const char* src_reg = getRegisterName(reg_map[op2]);
            fprintf(output, "\tcmpl %s, %s\n", src_reg, dest_reg);
        } else {
            if (offset_map.find(op2) != offset_map.end()) {
                int offset = offset_map[op2];
                fprintf(output, "\tcmpl %d(%%ebp), %s\n", offset, dest_reg);
            }
        }
    }

    // Set comparison result
    fprintf(output, "\t%s %%al\n", set_name);
    fprintf(output, "\tmovzbl %%al, %s\n", dest_reg);

    // Store result if it goes to memory
    if (reg_map.find(instr) == reg_map.end() || reg_map[instr] == SPILLED) {
        if (offset_map.find(instr) != offset_map.end()) {
            int offset = offset_map[instr];
            fprintf(output, "\tmovl %s, %d(%%ebp)\n", dest_reg, offset);
        }
    }
}

/*
 * Main assembly generation function
 */
int generateAssembly(LLVMModuleRef module,
                    std::unordered_map<LLVMValueRef, int>& reg_map,
                    const char* output_filename) {

    FILE* output = fopen(output_filename, "w");
    if (!output) {
        printf("Error: cannot open output file %s\n", output_filename);
        return 1;
    }

    // Process each function in the module
    LLVMValueRef func = LLVMGetFirstFunction(module);
    while (func) {
        if (!LLVMIsDeclaration(func)) {
            // Create basic block labels
            BBLabelMap bb_labels;
            createBBLabels(func, bb_labels);

            // Print directives
            printDirectives(func, output);

            // Compute offset map
            OffsetMap offset_map;
            int localMem;
            getOffsetMap(func, offset_map, localMem);

            // Function prologue
            emitOp(output, "pushl", "%ebp");
            emitOp(output, "movl", "%esp", "%ebp");
            fprintf(output, "\tsubl $%d, %%esp\n", localMem);
            emitOp(output, "pushl", "%ebx");

            // Process each basic block
            for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(func);
                 bb != NULL;
                 bb = LLVMGetNextBasicBlock(bb)) {

                // Print basic block label
                if (bb_labels.find(bb) != bb_labels.end()) {
                    fprintf(output, "%s:\n", bb_labels[bb].c_str());
                }

                // Process each instruction
                for (LLVMValueRef instr = LLVMGetFirstInstruction(bb);
                     instr != NULL;
                     instr = LLVMGetNextInstruction(instr)) {

                    LLVMOpcode opcode = LLVMGetInstructionOpcode(instr);

                    switch (opcode) {
                        case LLVMRet:
                            generateReturn(instr, reg_map, offset_map, output);
                            emitOp(output, "popl", "%ebx");
                            printFunctionEnd(output);
                            break;
                        case LLVMLoad:
                            generateLoad(instr, reg_map, offset_map, output);
                            break;
                        case LLVMStore:
                            generateStore(instr, reg_map, offset_map, output);
                            break;
                        case LLVMCall:
                            generateCall(instr, reg_map, offset_map, output);
                            break;
                        case LLVMBr:
                            generateBranch(instr, bb_labels, reg_map, offset_map, output);
                            break;
                        case LLVMAdd:
                        case LLVMSub:
                        case LLVMMul:
                            generateArithmetic(instr, reg_map, offset_map, output);
                            break;
                        case LLVMICmp:
                            generateCompare(instr, reg_map, offset_map, output);
                            break;
                        default:
                            /* Other instruction types not yet implemented */
                            break;
                    }
                }
            }
        }

        func = LLVMGetNextFunction(func);
    }

    fclose(output);
    return 0;
}