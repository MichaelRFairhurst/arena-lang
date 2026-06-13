# Arena Compiler

This compiler implements the Arena programming language, a rust-like C alternative for developers who like C.

# Mission Statement

Arena aims to improve C by:
- fixing bad defaults (e.g., implicit conversions)
- natively support C interoperability as a top priority
- encouraging simple, compile-time checked memory management via first class arenas
- slightly improve syntax woes & ergonomics

while maintaining a simple, minimal language design and fast compile speeds, and a minimal optional runtime.

## Arenas

Arenas offer a simple memory management model that is easy to understand and use and highly performant. Large scale C and C++ projects such as LLVM rely heavily on arenas.

Rather than tediously matching every `malloc` with a `free`, arenas offer developers a place to allocate scratch memory for some stage of program execution that can be cleaned up in a single call. In practice, arenas work much like call stack memory, but without a fixed sized.

The `arena` keyword begins a new scoped arena.
```
fun f() {
    arena {
        // all allocations are scoped to this block.
    }
}
```

Allocations within the arena are cleared when the block exits -- including allocations by other functions.
```
fun g() {
    let p: new int(); // `p` is not leaked; it is added to the caller's arena.

    arena { g(); }    // Recurse, with a new arena for each call.
}
```

The lifetime model of arenas is as simple as stack memory, so these lifetimes are checked at compile time via lifetime annotations and inference.
```
// A function which returns a pointer to an int allocated in the current arena.
fun f() -> int *arena {
    let x = 0;   // stack variable
    let p = x.&; // pointer to stack

    ret p;       // compile error: stack lifetime is shorter than arena lifetime
}

// A function which copies an int from one arena to another.
fun copy_int(src: int *a, dst: int *b) {
    dest = src;     // compile error: pointers do not have the same lifetime
    dest.* = src.*; // allowed: we are copying the value, not the pointer
}
```

Lifetime annotations are powerful and flexible, but they're also optional. Unspecified lifetimes are inferred by the compiler, and the `*unsafe` lifetime can be used to bypass lifetime checks.

## Ergonomics

Arena syntax is not too different from C. The main differences are:
- Variable declarations use `let name: type = init` syntax, which has fewer parsing ambiguities
- Types always read right-to-left: `T[N]` for an array, `T*` for a pointer, `T *lifetime`, and `T*[N]` for an array of pointers, etc.
- Mixing prefix operators with postfix operators is messy, e.g. `&((int) (*ptr->field)[n])`. Expressions in arena are always left-to-right: `ptr..field.*[n].as(int).&`.

Arena also offers simple and fast type inference. Arena uses bidirectional type inference, which requires no complex constraint solving and doesn't give complex chain-of-deductions errors. _Note that _lifetimes_ do use very simple constraint solving, but even this is just a topological sort._

### Multiple return values without tuples or a new calling convention

C already has a method of returning multiple values with out-arg pointers. Arena supports multiple return values as a syntax layer on top of this pattern.

```
let display, err = display_init(display.&);
```

Here we declare the variables `display` and `err`. The type of `display` is inferred by the parameter type of `display_init(d)` which takes a pointer to a `Display` (and initializes it).

While most useful for initialization and C-interop, it can be used for general multiple return values as well.
```
let x, y in coord_parts(coord, x.&, y.&);
```

### ABI-transparent generics

Arena allows functions to declare _types_ as function arguments. At runtime, this passes a size, allowing c-interoperable generic functions without any new calling conventions or runtime support.

```
fun alloc_array(size: int, type T) -> T* { ... }

void f() {
    // Allocate an array with an explicit type
    let arr1 = alloc_array(5, sizeof(int));

    // Allocate an array with an inferred type
    let arr2: int* = alloc_array(5);

    // Allocate an array with a dynamic type
    let some_size = ...;
    let arr3: void* = alloc_array(5, some_size);
}
```

Structs can also be generic.
```
struct Linked<T> {
    data: T*;
    next: Linked<T>?*;
};
```

### Other ergonomics features

- defer statements
- namespaces
- unified call syntax (`f.x()` as sugar for `f(x)`)
- simpler package model (no separate header files, except as needed for C interop)

Coming soon: typesafe union types, type traits as data, and more ergonomic error handling.

## Safety

Memory safety generally involves ensuring that a value holds the data it is supposed to hold, for the lifetime it is usable. This usually means
- Initializing memory before use
- Only creating/dereferencing valid pointers
- Not using memory after it has been freed

Arena does _not_ attempt to solve exclusive mutability, but it does attempt to solve the other three issues listed.

Arena also improves functional safety by abandoning poor defaults from C such as implicit conversions and widening of integer types.

### Initialization safety

Arena does not rely on zero-initialization to ensure memory safety. However, uninitialized memory may not be used _by default_.

```
let x = 0;    // Allowed
x + 1;        // Allowed: x is initialized

let y: int;         // Allowed
y + 1;              // Not allowed: y is uninitialized
y.unsafe(init) + 1; // allowed.

y = 0;        // Allowed: y is now initialized
y + 1;        // Allowed: y is initialized
```

Variables can also be initialized via pointers. This is required for structs.
```
struct MyStruct {
    a: int;

    fun my_initialize(@init self) {
        self..a = 0;
    }
};

fun f() {
    // Declare and initialize `MyStruct` in one line.
    let s1 in my_initialize(s.&);

    // Declare, allocate, and initialize `MyStruct` in one line.
    let new s2 in my_initialize(s2);
}
```

The compiler also ensures that errors returned by initialization functions are handled, so that initialization cannot be accidentally skipped.

```
let s1, err = my_initialize(s1.&); // s1 is not considered initialized

let s2, err = my_initialize(s2.&) except ret err; // Good: s2 is definitely initialized.
```

### Pointer validation

Arena offers both null-safe pointers and slices to ensure pointer validity.

```
let p1: int* = null;  // Error: pointers are not null by default
let p2: int?* = null; // Ok: `?` indicates a nullable pointer

p1.*; // Ok, non-null pointer
p2.*; // Error: p2 is nullable

p2
  .!(ret ERR_NULL_POINTER) // Returns err if `p2` is null
  .*;                      // Ok: dereferencing null-checked pointer

// Bypassed with unsafe:
p2.unsafe* + 1;
```

Arrays and slices have checked index operations by default. Slice types are pointer+length pairs declared with `T[]`.

```
let arr = [1, 2, 3]; // an array of 3 ints

arr[0];                  // Ok: constant index is in bounds
arr[x];                  // OK: but produces null if out of bounds
arr[x].!(ret ERR_OOB).*; // Ok: possibly null result is checked and dereferenced.
arr[x ! ret OOB];        // Alternative syntax for the above.

let slice = arr.as(int[]); // slice is a pointer to the first element of arr
slice[x];                  // as above
slice[x].!(ret ERR_OOB).*; // as above
slice[x ! ret OOB];        // as above

// bypassed with unsafe:
arr.unsafe[x];
slice.unsafe[x];
```

### Memory safety via arenas

Usually, memory is not freed in Arena except by exiting an arena and stack unwinding. The static type system ensures that pointers cannot outlive the arena they were allocated in, so memory safety is guaranteed by the arena model.

Most arenas use bump allocation, which is very fast but does not support deallocation. However, it is possible to use your own arena allocators and support deallocation, or to use `malloc` and `free` directly.

At the moment, deallocation is not checked when going outside of the arena model. Future versions plan to support linear types and move semantics, which would allow for statically checked memory deallocation without a GC.

## Custom arenas

The runtime is entirely replacable. Replacing the runtime can allow disabling of dynamic memory allocation entirely, or support for custom allocators, or even a GC.

## Metaprogramming

Arena plans support for some `contexpr` style compile-time expression evaluation in the future. However, it is our opinion that C's metaprogramming model via the preprocessor is already quite good. The preprocessor is simple, and fast, and powerful.

The C preprocessor is a bad replacement for generics, traits, typesafe unions, and compile time expression evaluation. However, it is a great tool for simple code generation and conditional compilation. Arena supports the C preprocessor and allows sharing of macros across projects.

## Lightweight runtime

The current Arena runtime is ~4kb in x86-64, and contains absolutely nothing except a default bump allocator. The runtime is entirely optional and replacable.

The language compiles to LLVM, and can run on any hardware supported by the LLVM project, or vendors with their own LLVM backends.

### Build Prerequisites

Building the arena compiler requires:

- vcpkg (for dependency management)
