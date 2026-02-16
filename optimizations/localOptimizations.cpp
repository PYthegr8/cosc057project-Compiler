/*
 * localOptimizations.cpp
 *
 * Implements local optimizations on LLVM IR functions.
 * Passes in this file:
 *  1) Constant Folding for integer add sub mul with constant operands
 *  2) Dead Code Elimination for unused non side effect instructions
 */

#include <vector>
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

/*
 * Runs local optimizations on a function.
 */
bool runLocalOptimizations(LLVMValueRef function) {
  bool changed = false;

  changed |= constantFolding(function);
  changed |= deadCodeElimination(function);

  return changed;
}
