#include <atomic>

/*
   Not meant to be run. Compiled to assembly and inspected with objdump,
   to see directly where the compiler can satisfy an atomic store with a
   single inline instruction, and where it has to fall back to calling
   into libatomic instead.

   x86_64 can move 8 bytes in a single instruction normally, and 16 bytes
   with specific instructions on CPUs that support them. A type that fits
   inside that width can be stored or loaded atomically with one
   instruction, no lock, no function call. A type wider than the CPU can
   move in one instruction has no such instruction to use. The standard
   library implementation has to fall back to something else, typically
   a lock internally, exposed as a call into libatomic rather than an
   inline instruction.

   FourBytes and EightBytes should compile their store to a single
   instruction, no call instruction involved.

   SixteenBytes may or may not, depending on whether this CPU has the
   16 byte atomic instructions available and whether the compiler is
   told it can use them. Without specific support, expect this one to
   fall back to a libatomic call as well.

   ThirtyTwoBytes is well past anything x86_64 can move in one
   instruction. Expect this one to compile to a call into libatomic,
   visible in the disassembly as a call instruction rather than an
   inline mov or lock-prefixed instruction.
*/

struct FourBytes   { int data[1]; };  // 4 bytes
struct EightBytes  { int data[2]; };  // 8 bytes
struct SixteenBytes{ int data[4]; };  // 16 bytes
struct ThirtyTwoBytes { int data[8]; }; // 32 bytes, same as Big in lock-free-check.cpp

std::atomic<FourBytes> a4;
std::atomic<EightBytes> a8;
std::atomic<SixteenBytes> a16;
std::atomic<ThirtyTwoBytes> a32;

void store_4()  { a4.store({}, std::memory_order_relaxed); }
void store_8()  { a8.store({}, std::memory_order_relaxed); }
void store_16() { a16.store({}, std::memory_order_relaxed); }
void store_32() { a32.store({}, std::memory_order_relaxed); }
