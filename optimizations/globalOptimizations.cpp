/*
 * globalOptimizations.cpp
 *
 * Implements global constant propagation using reaching store instructions.
 * Tracks stores with GEN/KILL/IN/OUT per basic block, then replaces loads when
 * all reaching stores write the same constant to the same address.
 */

#include <vector>
#include <set>
#include <unordered_map>

#include <llvm-c/Core.h>

static bool isStore(LLVMValueRef I) {
  return LLVMGetInstructionOpcode(I) == LLVMStore;
}

static bool isLoad(LLVMValueRef I) {
  return LLVMGetInstructionOpcode(I) == LLVMLoad;
}

static LLVMValueRef storePointer(LLVMValueRef storeInst) {
  return LLVMGetOperand(storeInst, 1);
}

static LLVMValueRef storeValue(LLVMValueRef storeInst) {
  return LLVMGetOperand(storeInst, 0);
}

static LLVMValueRef loadPointer(LLVMValueRef loadInst) {
  return LLVMGetOperand(loadInst, 0);
}

static bool isConstantStore(LLVMValueRef storeInst) {
  LLVMValueRef v = storeValue(storeInst);
  return LLVMIsAConstantInt(v) != nullptr;
}

static long long constantStoreValue(LLVMValueRef storeInst) {
  return LLVMConstIntGetSExtValue(storeValue(storeInst));
}

static void removeStoresToPointer(std::set<LLVMValueRef>& storeSet, LLVMValueRef ptr) {
  for (auto it = storeSet.begin(); it != storeSet.end(); ) {
    LLVMValueRef s = *it;
    if (storePointer(s) == ptr) it = storeSet.erase(it);
    else ++it;
  }
}

static std::set<LLVMValueRef> setUnion(const std::set<LLVMValueRef>& a,
                                       const std::set<LLVMValueRef>& b) {
  std::set<LLVMValueRef> out = a;
  out.insert(b.begin(), b.end());
  return out;
}

static std::set<LLVMValueRef> setDifference(const std::set<LLVMValueRef>& a,
                                            const std::set<LLVMValueRef>& b) {
  std::set<LLVMValueRef> out;
  for (LLVMValueRef x : a) {
    if (b.find(x) == b.end()) out.insert(x);
  }
  return out;
}

static bool setsEqual(const std::set<LLVMValueRef>& a, const std::set<LLVMValueRef>& b) {
  if (a.size() != b.size()) return false;
  auto itA = a.begin();
  auto itB = b.begin();
  while (itA != a.end()) {
    if (*itA != *itB) return false;
    ++itA; ++itB;
  }
  return true;
}

/*
 * Builds predecessor lists for each basic block by scanning terminator successors.
 */
static std::unordered_map<LLVMBasicBlockRef, std::vector<LLVMBasicBlockRef>>
buildPredecessors(LLVMValueRef function, const std::vector<LLVMBasicBlockRef>& blocks) {

  std::unordered_map<LLVMBasicBlockRef, std::vector<LLVMBasicBlockRef>> preds;

  for (LLVMBasicBlockRef b : blocks) {
    preds[b] = {};
  }

  for (LLVMBasicBlockRef b : blocks) {
    LLVMValueRef term = LLVMGetBasicBlockTerminator(b);
    if (!term) continue;

    unsigned numSucc = LLVMGetNumSuccessors(term);
    for (unsigned i = 0; i < numSucc; i++) {
      LLVMBasicBlockRef s = LLVMGetSuccessor(term, i);
      preds[s].push_back(b);
    }
  }

  return preds;
}

/*
 * Computes GEN and KILL sets for all basic blocks in the function.
 */
static void computeGenKill(
    LLVMValueRef function,
    const std::vector<LLVMBasicBlockRef>& blocks,
    const std::set<LLVMValueRef>& allStores,
    std::unordered_map<LLVMBasicBlockRef, std::set<LLVMValueRef>>& gen,
    std::unordered_map<LLVMBasicBlockRef, std::set<LLVMValueRef>>& kill) {

  // GEN: last store per address within the block
  for (LLVMBasicBlockRef b : blocks) {
    std::set<LLVMValueRef> genSet;

    for (LLVMValueRef I = LLVMGetFirstInstruction(b);
         I != nullptr;
         I = LLVMGetNextInstruction(I)) {

      if (!isStore(I)) continue;

      LLVMValueRef ptr = storePointer(I);
      removeStoresToPointer(genSet, ptr);
      genSet.insert(I);
    }

    gen[b] = genSet;
  }

  // KILL: for each store in B, kill all other stores to same address in function
  for (LLVMBasicBlockRef b : blocks) {
    std::set<LLVMValueRef> killSet;

    for (LLVMValueRef I = LLVMGetFirstInstruction(b);
         I != nullptr;
         I = LLVMGetNextInstruction(I)) {

      if (!isStore(I)) continue;

      LLVMValueRef ptr = storePointer(I);
      for (LLVMValueRef s : allStores) {
        if (s == I) continue;
        if (storePointer(s) == ptr) killSet.insert(s);
      }
    }

    kill[b] = killSet;
  }
}

/*
 * Computes IN and OUT sets for all basic blocks using iterative fixpoint.
 */
static void computeInOut(
    const std::vector<LLVMBasicBlockRef>& blocks,
    const std::unordered_map<LLVMBasicBlockRef, std::vector<LLVMBasicBlockRef>>& preds,
    const std::unordered_map<LLVMBasicBlockRef, std::set<LLVMValueRef>>& gen,
    const std::unordered_map<LLVMBasicBlockRef, std::set<LLVMValueRef>>& kill,
    std::unordered_map<LLVMBasicBlockRef, std::set<LLVMValueRef>>& in,
    std::unordered_map<LLVMBasicBlockRef, std::set<LLVMValueRef>>& out) {

  // Init IN empty, OUT = GEN
  for (LLVMBasicBlockRef b : blocks) {
    in[b] = {};
    out[b] = gen.at(b);
  }

  // Iterative fixpoint
  while (true) {
    bool changed = false;

    for (LLVMBasicBlockRef b : blocks) {
      std::set<LLVMValueRef> newIn;

      // IN[B] = union of OUT[pred]
      auto itPreds = preds.find(b);
      if (itPreds != preds.end()) {
        for (LLVMBasicBlockRef p : itPreds->second) {
          newIn = setUnion(newIn, out[p]);
        }
      }

      // OUT[B] = GEN[B] union (IN[B] - KILL[B])
      std::set<LLVMValueRef> newOut =
          setUnion(gen.at(b), setDifference(newIn, kill.at(b)));

      if (!setsEqual(newIn, in[b]) || !setsEqual(newOut, out[b])) {
        in[b] = newIn;
        out[b] = newOut;
        changed = true;
      }
    }

    if (!changed) break;
  }
}

/*
 * Constant propagation using store-load reaching stores.
 * Replaces loads if all reaching stores to the same address store the same constant.
 */
bool constantPropagation(LLVMValueRef function) {
  bool changed = false;

  // Collect blocks in a stable order
  std::vector<LLVMBasicBlockRef> blocks;
  for (LLVMBasicBlockRef b = LLVMGetFirstBasicBlock(function);
       b != nullptr;
       b = LLVMGetNextBasicBlock(b)) {
    blocks.push_back(b);
  }

  // Collect all store instructions in the function
  std::set<LLVMValueRef> allStores;
  for (LLVMBasicBlockRef b : blocks) {
    for (LLVMValueRef I = LLVMGetFirstInstruction(b);
         I != nullptr;
         I = LLVMGetNextInstruction(I)) {
      if (isStore(I)) allStores.insert(I);
    }
  }

  // Build preds and compute GEN/KILL/IN/OUT
  auto preds = buildPredecessors(function, blocks);

  std::unordered_map<LLVMBasicBlockRef, std::set<LLVMValueRef>> gen, kill, in, out;
  computeGenKill(function, blocks, allStores, gen, kill);
  computeInOut(blocks, preds, gen, kill, in, out);

  // Walk each block and replace loads using running reaching set R
  for (LLVMBasicBlockRef b : blocks) {
    std::set<LLVMValueRef> R = in[b];
    std::vector<LLVMValueRef> loadsToDelete;

    for (LLVMValueRef I = LLVMGetFirstInstruction(b);
         I != nullptr;
         I = LLVMGetNextInstruction(I)) {

      if (isStore(I)) {
        LLVMValueRef ptr = storePointer(I);
        removeStoresToPointer(R, ptr);
        R.insert(I);
        continue;
      }

      if (!isLoad(I)) continue;

      LLVMValueRef ptr = loadPointer(I);

      // Collect reaching stores to this same pointer
      std::vector<LLVMValueRef> reachingStores;
      for (LLVMValueRef s : R) {
        if (storePointer(s) == ptr) reachingStores.push_back(s);
      }

      if (reachingStores.empty()) continue;

      // Check all reaching stores are constant stores with the same constant
      bool ok = true;
      long long val = 0;

      for (size_t idx = 0; idx < reachingStores.size(); idx++) {
        LLVMValueRef s = reachingStores[idx];

        if (!isConstantStore(s)) {
          ok = false;
          break;
        }

        long long sVal = constantStoreValue(s);
        if (idx == 0) val = sVal;
        else if (sVal != val) {
          ok = false;
          break;
        }
      }

      if (!ok) continue;

      // Replace load with constant and mark for deletion
      LLVMTypeRef loadTy = LLVMTypeOf(I);
      LLVMValueRef c = LLVMConstInt(loadTy, (unsigned long long)val, 1);

      LLVMReplaceAllUsesWith(I, c);
      loadsToDelete.push_back(I);
      changed = true;
    }

    // Delete marked loads after traversal
    for (LLVMValueRef l : loadsToDelete) {
      LLVMInstructionEraseFromParent(l);
    }
  }

  return changed;
}
