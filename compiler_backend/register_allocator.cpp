/*
 * File Name: register_allocator.cpp
 * Description: Implementation of register allocation using linear-scan algorithm
 * Author: Student
 */

#include "register_allocator.h"
#include <cassert>
#include <algorithm>
#include <iostream>

/*
 * Helper function to count how many times a value is used in a basic block
 */
static int count_uses(LLVMValueRef val, LLVMBasicBlockRef bb) {
    int count = 0;
    
    for (LLVMValueRef instr = LLVMGetFirstInstruction(bb);
         instr != NULL;
         instr = LLVMGetNextInstruction(instr)) {
        
        /* Count this value if it's an operand of instr */
        int num_operands = LLVMGetNumOperands(instr);
        for (int i = 0; i < num_operands; i++) {
            if (LLVMGetOperand(instr, i) == val) {
                count++;
            }
        }
    }
    
    return count;
}

/*
 * Helper function to check if two liveness ranges overlap
 */
static bool ranges_overlap(std::pair<int, int> range1, std::pair<int, int> range2) {
    /* Two ranges [a,b] and [c,d] overlap if NOT (b < c or d < a) */
    return !(range1.second < range2.first || range2.second < range1.first);
}

/*
 * Helper function to check if an instruction has a result/LHS
 * (i.e., it defines a value)
 */
static bool has_result(LLVMValueRef instr) {
    LLVMOpcode opcode = LLVMGetInstructionOpcode(instr);
    
    /* Instructions with no result: store, branch, return, etc */
    switch (opcode) {
        case LLVMStore:
        case LLVMBr:
        case LLVMRet:
        case LLVMCall:
            /* Note: some calls may return void */
            return LLVMTypeOf(instr) != LLVMVoidType();
        default:
            return true;
    }
}

/*
 * Compute liveness information for a basic block
 * This analyzes where each value is born (defined) and where it dies (last use)
 */
void compute_liveness(LLVMBasicBlockRef bb, 
                      IndexMap& inst_index, 
                      LivenessMap& live_range) {
    /* Clear previous data */
    inst_index.clear();
    live_range.clear();
    
    int index = 0;
    
    /* First pass: build instruction index (skip alloca) */
    for (LLVMValueRef instr = LLVMGetFirstInstruction(bb);
         instr != NULL;
         instr = LLVMGetNextInstruction(instr)) {
        
        /* Skip alloca instructions */
        if (LLVMGetInstructionOpcode(instr) == LLVMAlloca) {
            continue;
        }
        
        /* Map this instruction to its index */
        inst_index[instr] = index;
        index++;
    }
    
    /* Second pass: compute liveness ranges */
    for (LLVMValueRef instr = LLVMGetFirstInstruction(bb);
         instr != NULL;
         instr = LLVMGetNextInstruction(instr)) {
        
        /* Skip alloca instructions */
        if (LLVMGetInstructionOpcode(instr) == LLVMAlloca) {
            continue;
        }
        
        /* Skip instructions with no result */
        if (!has_result(instr)) {
            continue;
        }
        
        /* Start index: where this instruction is (it defines the value) */
        int start = inst_index[instr];
        int end = start;
        
        /* Find the last use of this value in the basic block */
        for (LLVMValueRef use_instr = LLVMGetFirstInstruction(bb);
             use_instr != NULL;
             use_instr = LLVMGetNextInstruction(use_instr)) {
            
            /* Skip alloca */
            if (LLVMGetInstructionOpcode(use_instr) == LLVMAlloca) {
                continue;
            }
            
            /* Check if use_instr uses instr as an operand */
            int num_operands = LLVMGetNumOperands(use_instr);
            for (int i = 0; i < num_operands; i++) {
                if (LLVMGetOperand(use_instr, i) == instr) {
                    /* Found a use, update end if this instruction is later */
                    if (inst_index.count(use_instr)) {
                        end = std::max(end, inst_index[use_instr]);
                    }
                    break;
                }
            }
        }
        
        /* Store the liveness range for this instruction */
        live_range[instr] = std::make_pair(start, end);
    }
}

/*
 * Find a candidate value to spill (evict from register to memory)
 * Uses a simple heuristic: find first overlapping instruction with a register
 */
LLVMValueRef find_spill(LLVMValueRef instr,
                        RegisterMap& reg_map,
                        IndexMap& inst_index,
                        std::vector<LLVMValueRef>& sorted_list,
                        LivenessMap& live_range) {
    
    /* Get liveness range of the instruction we're trying to allocate */
    if (live_range.find(instr) == live_range.end()) {
        return NULL;
    }
    
    std::pair<int, int> instr_range = live_range[instr];
    
    /* Look through sorted list for overlapping instruction with register */
    for (LLVMValueRef candidate : sorted_list) {
        
        /* Check if candidate has a register assigned */
        if (reg_map.find(candidate) == reg_map.end() || reg_map[candidate] == SPILLED) {
            continue;
        }
        
        /* Check if their live ranges overlap */
        if (live_range.find(candidate) != live_range.end()) {
            if (ranges_overlap(instr_range, live_range[candidate])) {
                /* Found a candidate with overlapping liveness and assigned register */
                return candidate;
            }
        }
    }
    
    return NULL;
}

/*
 * Main register allocation function
 * Processes each basic block independently and allocates registers
 */
RegisterMap allocate_registers(LLVMValueRef func) {
    RegisterMap final_reg_map;
    
    /* Process each basic block in the function */
    for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(func);
         bb != NULL;
         bb = LLVMGetNextBasicBlock(bb)) {
        
        /* Local maps for this basic block */
        IndexMap inst_index;
        LivenessMap live_range;
        RegisterMap reg_map;
        
        /* Compute liveness for this basic block */
        compute_liveness(bb, inst_index, live_range);
        
        /* Initialize set of available registers */
        std::set<int> available_registers;
        available_registers.insert(REG_EBX);
        available_registers.insert(REG_ECX);
        available_registers.insert(REG_EDX);
        
        /* Process each instruction in the basic block */
        for (LLVMValueRef instr = LLVMGetFirstInstruction(bb);
             instr != NULL;
             instr = LLVMGetNextInstruction(instr)) {
            
            /* Skip alloca instructions */
            if (LLVMGetInstructionOpcode(instr) == LLVMAlloca) {
                continue;
            }
            
            /* Handle instructions with no result (store, br, etc) */
            if (!has_result(instr)) {
                
                /* Free up registers if any operand's liveness ends */
                int num_operands = LLVMGetNumOperands(instr);
                for (int i = 0; i < num_operands; i++) {
                    LLVMValueRef operand = LLVMGetOperand(instr, i);
                    
                    if (live_range.find(operand) != live_range.end()) {
                        if (inst_index.find(instr) != inst_index.end()) {
                            int current_index = inst_index[instr];
                            int end = live_range[operand].second;
                            
                            /* If this operand's liveness ends at this instruction */
                            if (end == current_index) {
                                if (reg_map.find(operand) != reg_map.end() && 
                                    reg_map[operand] != SPILLED) {
                                    /* Free the register */
                                    available_registers.insert(reg_map[operand]);
                                }
                            }
                        }
                    }
                }
                
                continue;
            }
            
            /* Now handle instructions that produce a result */
            
            int current_index = inst_index[instr];
            LLVMOpcode opcode = LLVMGetInstructionOpcode(instr);
            
            /* Check if this is an arithmetic instruction (add, sub, mul, div) */
            bool is_arithmetic = (opcode == LLVMAdd || opcode == LLVMSub || 
                                 opcode == LLVMMul || opcode == LLVMSDiv);
            
            if (is_arithmetic) {
                /* Try to reuse first operand's register if its liveness ends */
                LLVMValueRef first_op = LLVMGetOperand(instr, 0);
                
                if (reg_map.find(first_op) != reg_map.end() && 
                    reg_map[first_op] != SPILLED) {
                    
                    if (live_range.find(first_op) != live_range.end()) {
                        if (live_range[first_op].second == current_index) {
                            /* First operand's liveness ends here, reuse its register */
                            int reg = reg_map[first_op];
                            reg_map[instr] = reg;
                            
                            /* Free second operand's register if its liveness ends */
                            if (LLVMGetNumOperands(instr) > 1) {
                                LLVMValueRef second_op = LLVMGetOperand(instr, 1);
                                if (reg_map.find(second_op) != reg_map.end() && 
                                    reg_map[second_op] != SPILLED) {
                                    if (live_range.find(second_op) != live_range.end()) {
                                        if (live_range[second_op].second == current_index) {
                                            available_registers.insert(reg_map[second_op]);
                                        }
                                    }
                                }
                            }
                            
                            /* Add register back to available if it wasn't reused */
                            available_registers.erase(reg);
                            continue;
                        }
                    }
                }
            }
            
            /* Try to allocate a free register */
            if (!available_registers.empty()) {
                /* Get a register from available pool */
                int reg = *available_registers.begin();
                available_registers.erase(available_registers.begin());
                
                reg_map[instr] = reg;
                
                /* Free registers if operands' liveness ends */
                int num_operands = LLVMGetNumOperands(instr);
                for (int i = 0; i < num_operands; i++) {
                    LLVMValueRef operand = LLVMGetOperand(instr, i);
                    
                    if (live_range.find(operand) != live_range.end()) {
                        if (live_range[operand].second == current_index) {
                            if (reg_map.find(operand) != reg_map.end() && 
                                reg_map[operand] != SPILLED) {
                                available_registers.insert(reg_map[operand]);
                            }
                        }
                    }
                }
                
            } else {
                /* No registers available, need to spill */
                
                /* Create sorted list for spilling heuristic */
                std::vector<LLVMValueRef> sorted_list;
                for (auto& pair : reg_map) {
                    if (pair.second != SPILLED) {
                        sorted_list.push_back(pair.first);
                    }
                }
                
                /* Sort by liveness range end (decreasing order) */
                std::sort(sorted_list.begin(), sorted_list.end(),
                    [&live_range](LLVMValueRef a, LLVMValueRef b) {
                        int end_a = (live_range.find(a) != live_range.end()) ? 
                                   live_range[a].second : -1;
                        int end_b = (live_range.find(b) != live_range.end()) ? 
                                   live_range[b].second : -1;
                        return end_a > end_b;
                    });
                
                /* Find candidate to spill */
                LLVMValueRef to_spill = find_spill(instr, reg_map, inst_index, 
                                                   sorted_list, live_range);
                
                if (to_spill != NULL) {
                    /* Spill to_spill and give its register to instr */
                    int reg = reg_map[to_spill];
                    reg_map[instr] = reg;
                    reg_map[to_spill] = SPILLED;
                    
                } else {
                    /* No candidate to spill, mark instr as spilled */
                    reg_map[instr] = SPILLED;
                }
                
                /* Free registers if operands' liveness ends */
                int num_operands = LLVMGetNumOperands(instr);
                for (int i = 0; i < num_operands; i++) {
                    LLVMValueRef operand = LLVMGetOperand(instr, i);
                    
                    if (live_range.find(operand) != live_range.end()) {
                        if (live_range[operand].second == current_index) {
                            if (reg_map.find(operand) != reg_map.end() && 
                                reg_map[operand] != SPILLED) {
                                available_registers.insert(reg_map[operand]);
                            }
                        }
                    }
                }
            }
        }
        
        /* Merge this basic block's allocation into final result */
        for (auto& pair : reg_map) {
            final_reg_map[pair.first] = pair.second;
        }
    }
    
    return final_reg_map;
}
