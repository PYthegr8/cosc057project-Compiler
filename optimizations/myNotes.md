# Part 3 – Optimizer 

Started by separating files so things don’t get messy later.

runOptimizations.cpp -> just the driver  
localOptimizations.cpp -> constant folding, CSE, DCE  
globalOptimizations.cpp -> will handle constant propagation later

---

## Driver

I'm thinking i will 

Load module  
Loop over functions  
Run runLocalOptimizations(function)  
Print module

That’s it.

Originally had a segfault because I called LLVMDisposeMemoryBuffer after parsing.
LLVM already takes ownership during parsing, so that caused a double free.
Removed it and everything worked.

---

## Local Pass Order

constantFolding  
commonSubexpressionElimination  
deadCodeElimination

Folding first simplifies expressions.
CSE next removes duplicate work.
DCE last cleans everything that became unused.

---

## Constant Folding

Only fold add/sub/mul.

For each instruction:
- check opcode
- check both operands are constants
- compute new constant
- replace uses
- delete old instruction later

I never erase during iteration.
Always collect first, delete after.

Tests passed for:
cfold_add, cfold_sub, cfold_mul.

---

## CSE (first attempt)

First version only removed duplicate arithmetic instructions.
That worked partially but did not match p2_common_subexpr expected output.

Issue: loads were not being unified.

---

## CSE (updated version)

Added load handling.

Per basic block:

Track last load per pointer.
If I see:
- store ptr X -> invalidate previous load of X
- load ptr X -> if seen before and not invalidated, reuse earlier load

Then arithmetic CSE works naturally because operands become identical.

After replacing duplicate loads and adds, DCE removes dead instructions.

Now p2_common_subexpr matches reference behavior (except numbering/metadata).

---

## Dead Code Elimination

Delete instructions with:
- no uses
- not side-effecting

Side-effect instructions kept:
store, call, alloca, terminators, load

Loads are kept to avoid over-aggressive cleanup that changes output structure.

DCE runs in a loop because deleting one instruction can free others.

---

## Testing

Two ways I tested:

1) Diff IR

./optimizer input.ll > mine.ll  
diff mine.ll reference.ll

Differences observed:
- ModuleID path
- PIC Level metadata
- SSA numbering

These are LLVM version differences, not logic errors.

2) Compile and run

clang main.c mine.ll -o mine.out  
clang main.c reference.ll -o ref.out  
./mine.out  
./ref.out

Outputs matched -> semantics preserved.

---

## Observations

- SSA numbering changes when earlier instructions are deleted.
- LLVM metadata differs by version.
- Correctness should be judged by structure and runtime behavior, not raw numbering.

---

## Current State

Local optimizations working:
- constant folding
- CSE (including loads)
- DCE

Next step will be global constant propagation using store/load dataflow.
