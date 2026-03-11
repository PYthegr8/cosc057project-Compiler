/*
 * File Name: register_allocator.h
 * Description: Header file for register allocation using linear-scan algorithm
 * Author: Papa Yaw Owusu Nti
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
 * Analyze where values are defined and used in a basic block.
 * Assigns an index to each instruction and tracks which instructions use each value.
 * Alloca instructions are skipped and not indexed.
 */
void compute_liveness(LLVMBasicBlockRef bb, 
                      IndexMap& inst_index, 
                      LivenessMap& live_range);

/*
 * Find an instruction that can be spilled to memory when all registers are in use.
 * Looks for an instruction with overlapping liveness that already has a register assigned.
 */
LLVMValueRef find_spill(LLVMValueRef instr,
                        RegisterMap& reg_map,
                        IndexMap& inst_index,
                        std::vector<LLVMValueRef>& sorted_list,
                        LivenessMap& live_range);

/*
 * Allocate physical registers to all instructions in a function using linear-scan allocation.
 * Assigns registers from %ebx, %ecx, %edx. Spills values to stack memory when needed.
 */
RegisterMap allocate_registers(LLVMValueRef func);

#endif
