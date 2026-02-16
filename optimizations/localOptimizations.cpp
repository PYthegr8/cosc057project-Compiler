/*
 * localOptimizations.cpp
 *
 * Implements local optimizations on LLVM IR functions.
 * Passes in this file:
 *  1) Constant Folding for integer add sub mul with constant operands
 *  2) Dead Code Elimination for unused non side effect instructions
 */

#include <vector>
#include <unordered_map>
#include <llvm-c/Core.h>

/*
 * Returns true for instructions that must not be removed.
 * These affect memory or control flow.
 */
static bool isSideEffect(LLVMValueRef I) {
  LLVMOpcode op = LLVMGetInstructionOpcode(I);

  if (op == LLVMStore) return true;
  if (op == LLVMCall) return true;
  if (op == LLVMAlloca) return true;
  if (op == LLVMLoad) return true;
  if (LLVMIsATerminatorInst(I)) return true;

  return false;
}

/*
 * Constant Folding
 * Replaces add sub mul instructions when both operands are constants.
 */
bool constantFolding(LLVMValueRef function) {
  bool changed = false;
  std::vector<LLVMValueRef> toDelete;

  // Walk all basic blocks and instructions
  for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function);
       bb != nullptr;
       bb = LLVMGetNextBasicBlock(bb)) {

    for (LLVMValueRef I = LLVMGetFirstInstruction(bb);
         I != nullptr;
         I = LLVMGetNextInstruction(I)) {

      LLVMOpcode op = LLVMGetInstructionOpcode(I);
      if (!(op == LLVMAdd || op == LLVMSub || op == LLVMMul)) continue;

      LLVMValueRef a = LLVMGetOperand(I, 0);
      LLVMValueRef b = LLVMGetOperand(I, 1);

      // Only fold when both operands are constants
      if (!LLVMIsAConstant(a) || !LLVMIsAConstant(b)) continue;

      LLVMValueRef folded = nullptr;
      if (op == LLVMAdd) folded = LLVMConstAdd(a, b);
      else if (op == LLVMSub) folded = LLVMConstSub(a, b);
      else folded = LLVMConstMul(a, b);

      // Replace instruction uses and mark for deletion
      LLVMReplaceAllUsesWith(I, folded);
      toDelete.push_back(I);
      changed = true;
    }
  }

  // Remove folded instructions
  for (LLVMValueRef I : toDelete) {
    LLVMInstructionEraseFromParent(I);
  }

  return changed;
}

/*
 * Common Subexpression Elimination
 * Eliminates duplicate loads and duplicate add sub mul instructions within a basic block.
 * A load can be reused if no store writes to the same address in between.
 */
bool commonSubexpressionElimination(LLVMValueRef function) {
  bool changed = false;

  for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
       basicBlock != nullptr;
       basicBlock = LLVMGetNextBasicBlock(basicBlock)) {

    std::unordered_map<LLVMValueRef, LLVMValueRef> lastLoad;
    std::vector<LLVMValueRef> seenExprs;

    for (LLVMValueRef instruction = LLVMGetFirstInstruction(basicBlock);
         instruction != nullptr;
         instruction = LLVMGetNextInstruction(instruction)) {

      LLVMOpcode op = LLVMGetInstructionOpcode(instruction);

      // Store kills previous loads from that same address
      if (op == LLVMStore) {
        LLVMValueRef storePtr = LLVMGetOperand(instruction, 1);
        lastLoad.erase(storePtr);
        continue;
      }

      // Reuse repeated loads from the same address
      if (op == LLVMLoad) {
        LLVMValueRef loadPtr = LLVMGetOperand(instruction, 0);

        auto it = lastLoad.find(loadPtr);
        if (it != lastLoad.end()) {
          LLVMReplaceAllUsesWith(instruction, it->second);
          changed = true;
        } else {
          lastLoad[loadPtr] = instruction;
        }
        continue;
      }

      // Reuse repeated arithmetic expressions
      if (!(op == LLVMAdd || op == LLVMSub || op == LLVMMul))
        continue;

      LLVMValueRef operand0 = LLVMGetOperand(instruction, 0);
      LLVMValueRef operand1 = LLVMGetOperand(instruction, 1);

      for (LLVMValueRef prev : seenExprs) {
        if (LLVMGetInstructionOpcode(prev) != op) continue;

        if (LLVMGetOperand(prev, 0) == operand0 &&
            LLVMGetOperand(prev, 1) == operand1) {
          LLVMReplaceAllUsesWith(instruction, prev);
          changed = true;
          break;
        }
      }

      seenExprs.push_back(instruction);
    }
  }

  return changed;
}


/*
 * Dead Code Elimination
 * Removes instructions that have no uses and no side effects.
 */
bool deadCodeElimination(LLVMValueRef function) {
  bool changed = false;

  while (true) {
    std::vector<LLVMValueRef> toDelete;

    // Scan for unused instructions
    for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function);
         bb != nullptr;
         bb = LLVMGetNextBasicBlock(bb)) {

      for (LLVMValueRef I = LLVMGetFirstInstruction(bb);
           I != nullptr;
           I = LLVMGetNextInstruction(I)) {

        if (LLVMGetFirstUse(I) != nullptr) continue;
        if (isSideEffect(I)) continue;

        toDelete.push_back(I);
      }
    }

    // Stop if nothing to delete
    if (toDelete.empty()) break;

    // Delete collected instructions
    for (LLVMValueRef I : toDelete) {
      LLVMInstructionEraseFromParent(I);
    }

    changed = true;
  }

  return changed;
}
