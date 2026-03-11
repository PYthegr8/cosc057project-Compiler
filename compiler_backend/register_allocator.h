/*
 * File Name: register_allocator.h
 * Description: Header file for register allocation using linear-scan algorithm
 * Author: Student
 */

#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include <llvm-c/Core.h>
#include <map>
#include <unordered_map>
#include <vector>
#include <utility>
#include <set>

/* 
 * Physical register constants for x86-32
 * We have 3 available registers: ebx, ecx, edx
 */
#define REG_EBX 0
#define REG_ECX 1
#define REG_EDX 2
#define NUM_REGISTERS 3
#define SPILLED -1

/*
 * Data structure to store liveness information for a basic block
 * Maps LLVMValueRef (instructions) to their liveness range
 * Liveness range: (start_index, end_index) where the value is live
 */
typedef std::unordered_map<LLVMValueRef, std::pair<int, int>> LivenessMap;

/*
 * Data structure to map instructions to their index in a basic block
 * Index starts at 0 for first non-alloca instruction
 */
typedef std::unordered_map<LLVMValueRef, int> IndexMap;

/*
 * Register allocation result map
 * Maps LLVMValueRef (instructions that generate values) to assigned physical register
 * Value is register id (0=ebx, 1=ecx, 2=edx) or -1 for spilled
 */
typedef std::unordered_map<LLVMValueRef, int> RegisterMap;

/*
 * Compute liveness information for a basic block
 * 
 * Parameters:
 *   bb - The basic block to analyze
 *   inst_index - Output: maps instructions to their index in the block
 *   live_range - Output: maps instructions to their (start, end) liveness range
 *
 * Notes:
 *   - alloca instructions are skipped (not indexed)
 *   - All output maps are cleared at the beginning
 */
void compute_liveness(LLVMBasicBlockRef bb, 
                      IndexMap& inst_index, 
                      LivenessMap& live_range);

/*
 * Find a candidate value to spill (move to memory)
 * 
 * Parameters:
 *   instr - The instruction that needs a register
 *   reg_map - Current register allocation
 *   inst_index - Instruction indices in the basic block
 *   live_range - Liveness information for all values
 *   sorted_list - List of instructions sorted by some heuristic
 *
 * Returns:
 *   LLVMValueRef of instruction to spill, or NULL if none found
 *
 * Algorithm:
 *   - For each instruction in sorted_list with overlapping liveness
 *   - If it has a register assigned (not -1), return it
 *   - Return NULL if no candidates
 */
LLVMValueRef find_spill(LLVMValueRef instr,
                        RegisterMap& reg_map,
                        IndexMap& inst_index,
                        std::vector<LLVMValueRef>& sorted_list,
                        LivenessMap& live_range);

/*
 * Main register allocation function
 * Performs linear-scan register allocation on all basic blocks in a function
 * 
 * Parameters:
 *   func - The LLVM function to allocate registers for
 *
 * Returns:
 *   RegisterMap with complete allocation (includes all basic blocks)
 *   Maps LLVMValueRef to register id (0-2) or -1 for spilled
 */
RegisterMap allocate_registers(LLVMValueRef func);

#endif
