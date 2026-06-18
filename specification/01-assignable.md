
# Assignability

## Strictness

A type `t` is _const-strict_ if bitwise copying values of type `t` results in an effectively shallow copy:
- if `t` is a primitive type, then `t` is not const-strict.
- if `t` is `const T`, then `t` is not const-strict.
- if `t` is an array type `E[x]`, then `t` is const-strict if `E` is const-strict.
- if `t` is a pointer of type `T *l`, and `T` is not `const T'`, then `t` is const-strict.
- if `t` is a struct type with a field of type `T` where `T` is strict, then `t` is also strict.
- if `t` the `void` type, then `t` is not strict.

_Note: const-strictness ensures that a pointer to a const value cannot be treated is a pointer to a non-const value._

A type `t` is _lifetime-strict_ if contains writable pointers to memory with unfixed lifetimes.
- if `t` is a primitive type, then `t` is not lifetime-strict.
- if `t` is `const T`, then `t` is not lifetime-strict.
- if `t` is an array type `E[x]`, then `t` is lifetime-strict if `E` is lifetime-strict.
- if `t` is a pointer of type `T *l`, `T` is not `const T'`, then `t` is lifetime-strict.
- if `t` is a struct type with a field of type `T` where `T` is lifetime-strict, then `t` is also lifetime-strict. If `T` is `P *l` where `P` is not `const P'` and `l` is not `*static` or `*unsafe`, then `t` is also lifetime-strict.
- if `t` the `void` type, then `t` is not strict.

_Note: lifetime-strictness ensures that a value with a writable pointer of lifetime `*a` cannot be treated as a value with a writable pointer of lifetime `*b` where `*a != *b`. This would allow smuggling values with shorter lifetimes into longer lifetimes, which would be unsound._

Types that are not _const-strict_ are _const-permissive_, and types that are not _lifetime-strict_ are _lifetime-permissive_.

## Assignment rule

Assignment occurs within a context. That context may be _copying_ and/or _const_, or neither.

A value of type `a` and is assignable to a value of type `b` if:
- `a` and `b` are the same type.
- the context is _const_ and `a` is either `A` or `const A`, and `b` is either `B` or `const B` where `A` is assignable to `B` in a _const_ and equally _copying_ context.
- `a` is `T<a_0, ... a_n>` and `b` is `T<b_0, ..., b_n>` and `a_i` is exactly `b_i` for all `0..n`.
- a is `const A` and b is `const B`, and `A` is assignable to `B` in the same _context_.
- the context is _copying_, and `a` is `const A` and `b` is `B`, and `A` is assignable to `B` in a _copying_ and _const_ context.
- `a` is `A *a` and `b` is `B *b` an a _copying_ context, and `A` is assignable to `B` in a non-_copying_ and equally _const_ context. Additionally, if `B` is not _strict_ and the context is _copying_, then `*a >= *b` must hold, and otherwise `*a = *b` must hold.
- `a` is `A[n]` and `b` is `B[m]` and `A` is assignable to `B` in the same _context_.

The following examples assume a _copying_, non-_const_ context:
- `int` is assignable to `int`.
- `int` is assignable to `const int` and vice versa.
- `S const` with a single `int` member is assignable to `S`.
- `S const` with a member `int*` is not assignable to `S`.
- `int *` is assignable to `int const *`.
- `int *` is assignable to `int * const`.
- `int const *` is not assignable to `int *`.
- `int * const` is not assignable to `int *`.