This project builds a compiler pipeline:

1. Parse input program with Flex/Bison and build an AST
2. Semantic analysis
3. Variable renaming (unique names)
4. LLVM IR Builder (writes `output.ll`)
5. Optimizer (reads `output.ll`, writes optimized IR to `output_opt.ll`)

The builder tests are C files (p1.c, p2.c, …). They follow the restricted “miniC” rules used by our grammar.

## Build

Requires:
- clang++
- flex
- bison
- llvm-config-17

Build compiler and optimizer:

```bash
make compiler
make optimizer
````

Or build both by running:

```bash
make
```

## Run the compiler (produce LLVM IR)

Set the input file in the Makefile (variable `IN`) or pass it directly:

```bash
make run
```

This runs:

```bash
./compiler <input_file>
```

Output:

* `output.ll`

## Run the optimizer (optional)

After `output.ll` exists:

```bash
make opt
```

Output:

* `output_opt.ll`

## Running and comparing builder tests

The folder `llvm_builder/builder_tests/` contains reference programs like `p1.c`, `p2.c`, etc.

To compare behavior:

### 1) Reference output (normal C compile)

```bash
clang llvm_builder/builder_tests/p1.c -o ref
./ref > ref.txt
```

### 2) Compile with your compiler, then run LLVM output

```bash
./compiler llvm_builder/builder_tests/p1.c
clang output.ll run.c -o mine
./mine > mine.txt
diff ref.txt mine.txt
```

If diff prints nothing, the outputs match.

If you want to test optimized output instead:

```bash
./optimizer output.ll > output_opt.ll
clang output_opt.ll run.c -o mine_opt
./mine_opt > mine_opt.txt
diff ref.txt mine_opt.txt
```

## Clean

```bash
make clean
```