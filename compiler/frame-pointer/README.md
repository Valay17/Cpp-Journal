# The Frame Pointer: Check Your Default Before Reaching for the Flag

Full writeup: https://valay17.github.io/Portfolio/blog/compiler/frame-pointer

Every function call needs somewhere to put its local variables, its arguments, and the address to jump back to once it's done. That block of memory is a stack frame, and whether the compiler makes it easy to find depends on more than the optimization level, it depends on which compiler build is actually running.

Traditionally, a register called the frame pointer (`rbp` on x86-64) holds the address of the current function's frame, and each frame stores the previous function's frame pointer right alongside its own data. That creates a linked chain, walk from the current frame's saved `rbp` to the one before it, and the one before that, and the entire call stack is reconstructed, one function calling the next, all the way back to `main`.

Tools like `perf` reconstruct a call stack by walking exactly that chain. If it was never built, there's nothing to walk, stack traces come back broken or incomplete. `-fno-omit-frame-pointer` forces that chain to exist. The assumption most people carry is that this flag is needed because the default omits it. On this machine, that assumption is wrong.

## Three builds, not two

Compiling the same function three ways shows the actual picture.

Default `-O2` here already establishes a frame pointer, `mov rbp, rsp` right after the initial `push rbp`. Adding `-fno-omit-frame-pointer` produces byte-identical output, the flag does nothing, because there was nothing left to fix. The only way to get the traditionally-expected "fast" behavior, no frame pointer chain, `rbp` just pushed, popped, and reused as scratch space, is to pass `-fomit-frame-pointer` explicitly, the opposite of what most guidance says to add.

This isn't a quirk of this one machine, and confirming the actual mechanism matters more than assuming it. `echo $NIX_CFLAGS_COMPILE` on this environment shows no frame-pointer flags at all, just include paths, so that wasn't it. The answer came from checking the `cc1plus` invocation directly (`g++ -O2 -v -c codegen.cpp -o /dev/null`), which showed both `-fno-omit-frame-pointer` and `-mno-omit-leaf-frame-pointer` injected before the source file is ever touched. That pairing traces straight to Nixpkgs' own `cc-wrapper`, which echoes exactly those two flags into `cc-cflags-before` for every target except 32-bit x86 and s390, independent of whatever the underlying OS or GCC package would otherwise default to.

Ubuntu, Fedora, and Arch all made a related but separate decision at roughly the same time, rebuilding their own GCC packages to default to frame pointers system-wide, specifically to stop breaking `perf` and eBPF-based profiling. Ubuntu's change landed with 24.04 LTS, in collaboration with Polar Signals, cited at roughly 1-2% overhead. Fedora did the same starting with Fedora 38. The point isn't that any one of these is the reason, it's that this same conclusion has been reached independently by multiple ecosystems, distros and package managers alike, which means the traditional "-O2 omits the frame pointer" assumption is now wrong far more often than it's right, and the actual mechanism on any given machine is worth confirming directly rather than assumed from memory of how GCC used to behave.

`push rbp` and `pop rbp` alone don't prove a frame pointer exists either way, `rbp` is a general-purpose register and can be pushed purely to preserve it across a call without ever being set up as a frame pointer. The instruction that actually matters is `mov rbp, rsp`, right after the push. That line's presence or absence is the test that matters, not whether `rbp` appears in the disassembly at all.

## Key insight

```cpp
extern int bar(int x);

int foo(int a) {
    int b = a + 10;
    return bar(b) + bar(b + 1);
}
```

`bar` is external and undefined on purpose, so it cannot be inlined away, this is about what `foo` does with its own frame around two calls it cannot see inside of.

## Run: check your own default

```bash
echo $NIX_CFLAGS_COMPILE
```
If a Nix-based environment is injecting the flag through this variable, it will show up here directly. On this machine it does not, this variable only carries include paths, which is what ruled this mechanism out and pointed toward checking the actual compiler invocation instead.

```bash
g++ -O2 -v -c codegen.cpp -o /dev/null 2>&1 | grep "cc1plus" | tr ' ' '\n' | grep -i frame
```
This prints the literal internal `cc1plus` command GCC runs, one flag per line, filtered down to anything frame-related. Whatever shows up here is what the compiler is doing, regardless of what any wrapper, environment variable, or assumption about "how -O2 behaves" says.

## Run: default -O2

```bash
g++ -O2 -std=c++20 -c codegen.cpp -o codegen-o2.o
objdump -d -M intel --no-show-raw-insn codegen-o2.o
```
`-c` compiles to an object file without linking, since this file has no `main` and is not meant to run. `-M intel` selects Intel syntax. `--no-show-raw-insn` hides the raw instruction bytes. Check specifically for `mov rbp, rsp` right after the initial `push rbp`, its presence or absence is what determines whether this build already has a frame pointer chain, on some distros it will, on others it won't.

## Run: -fno-omit-frame-pointer

```bash
g++ -O2 -std=c++20 -fno-omit-frame-pointer -c codegen.cpp -o codegen-o2-fp.o
objdump -d -M intel --no-show-raw-insn codegen-o2-fp.o
```
If the default build already had `mov rbp, rsp`, expect this output to be identical to it. If the default build omitted it, expect this one to add it.

## Run: -fomit-frame-pointer

```bash
g++ -O2 -std=c++20 -fomit-frame-pointer -c codegen.cpp -o codegen-o2-omit.o
objdump -d -M intel --no-show-raw-insn codegen-o2-omit.o
```
The explicit opposite flag, forcing omission regardless of what the default happens to be. This is the build to compare against the other two to see what "omitted" actually looks like, `rbp` still pushed and popped, but reused as a plain scratch register, no `mov rbp, rsp` anywhere.

## Output

```
$ objdump -d -M intel --no-show-raw-insn codegen-o2.o

0000000000000000 <foo(int)>:
   0:   push   rbp
   1:   mov    rbp,rsp
   4:   push   r12
   6:   push   rbx
   7:   mov    ebx,edi
   9:   lea    edi,[rdi+0xa]
   c:   call   11 <foo(int)+0x11>
  11:   lea    edi,[rbx+0xb]
  14:   mov    r12d,eax
  17:   call   1c <foo(int)+0x1c>
  1c:   pop    rbx
  1d:   add    eax,r12d
  20:   pop    r12
  22:   pop    rbp
  23:   ret
```
`mov rbp,rsp` is already present, this build was not compiled with a traditional GCC default, frame pointers are already on.

```
$ objdump -d -M intel --no-show-raw-insn codegen-o2-fp.o

(identical to codegen-o2.o above, byte for byte)
```
`-fno-omit-frame-pointer` changes nothing here, confirming the default already matched what the flag asks for.

```
$ objdump -d -M intel --no-show-raw-insn codegen-o2-omit.o

0000000000000000 <foo(int)>:
   0:   push   rbp
   1:   push   rbx
   2:   mov    ebx,edi
   4:   lea    edi,[rdi+0xa]
   7:   sub    rsp,0x8
   b:   call   10 <foo(int)+0x10>
  10:   lea    edi,[rbx+0xb]
  13:   mov    ebp,eax
  15:   call   1a <foo(int)+0x1a>
  1a:   add    rsp,0x8
  1e:   add    eax,ebp
  20:   pop    rbx
  21:   pop    rbp
  22:   ret
```
No `mov rbp,rsp` anywhere. `rbp` is pushed at the top and popped at the bottom, but in between it holds the result of the first `bar` call, `mov ebp,eax`, plain scratch space, not a frame pointer. This is what "omitted" actually looks like, and it only shows up here because omission was forced explicitly.

## Environment

- CPU: AMD Ryzen 7 5700U
- Kernel: 6.8.0-124-generic
- Compiler: g++ (GCC) 16.1.0
