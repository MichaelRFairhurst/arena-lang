# Static analysis query infrastructure

The static analysis and typechecking is broken into individual queries executed by a caching query engine.

This markdown file documents somewhat how the query engine itself works, but mostly is intended to describe how the queries work together to deliver performant and incremental type checking that can be used by both the frontend and a future LSP.

## Query Engine Overview

The query engine simply declares a set of "queries," where:
- There is a static type representing the inputs to the query (typically, a filepath)
- There is a static type representing the outputs of the query
- The dependencies between each query are tracked when a query runs, rather than being declared upfront.
- The query engine uses `==`-equality on the output keys to determine if a query's result has changed.
- revalidation of a query's result is performed lazily

The above is performed by carefully tracking revision index. The query engine itself has a current revision, and each cached query result also tracks two revision indexes: the revision of last update, and the revision of last validation.

Whenever the engine executes a query (as opposed to using a cached value), it increments its current revision index. The completed query result is then compared against the previous cached result. If the query result is different than the cached result, then this new result is stored in the cache. This cached result has the current revision used for its "updated" revision and its "validated" revision. If the new result matches the cached one, then the new result is dropped, and the cached result's "validated" index is updated to the current revision.

Lastly, when a query is executed, it is passed a handle to the current engine. This allows the query to request the results of other queries during execution. Each time it does so, this dependency is tracked by the engine.

Requesting a query result from the engine works as follows:
- If there is no cached result, perform the query and cache it, updating revisions accordingly
- If there is a cached result, check if its "validate" revision is up-to-date with the current revision. If it is, return the cached result.
- If the "validated" revision is behind the current revision, then recursively validate the queries dependencies.
- If a dependency has changed since the requested query was last validated, then the current query must be re-executed
- If a dependency has not changed, but has not been validated at the current revision, recurse to validate it.
- If none of the dependencies have changed, set the current query's "validated" revision to the current revision and return the cached result.

Requesting a high-level result such as the final type-checked AST will trigger multiple layers of queries. For example, the type-checked AST query will request imported symbol information, which in turn requests that a given file is parsed, which in turn requests the raw file contents.

The lowest level (raw file contents) uses slightly different mechanism. Currently, the raw file contents are always read. However, in the future, this can hold a file overlap to support an LSP server that's invalidated on certain LSP events, and/or use file-watching to invalidate the file contents cache when the underlying files change.

## Static analysis query stages

Roughly speaking, there are six passes implemented as queries:
- Lexing and parsing (in one pass)
- Import collection
- Typename collection
- Function name collection
- Expression resolution
- Type checking

Likely to optimize performance, we'll want to make one collection pass for all the names (imported symbols, typenames, and function names) rather than having separate passes for each.

### Semantic IDs

Our static analysis process involves assigning global canonical IDs to various entities such as types, functions, and imported symbols.

If we take a file like:

```arena
import foo

struct MyStruct {}

fun my_func(param: int) -> SomeStruct* {
    return foo_get_struct()
}
```

We will intern the various strings and give them unique IDs, for instance:

- Functions: 'my_func', 'foo_get_struct'
- Struct names: 'MyStruct', 'SomeStruct'

_Types_ are slightly more complicated. The simplest case is named types, such as `MyStruct`,
`SomeStruct`, and `int`. These are assigned unique IDs in the same way as functions and struct
names. We also create "symbols" for compound types, such as pointers and arrays. While `MyStruct*`
could be directly represented as a symbol, this would on lookup require resolving the entire
`MyStruct*` type. Instead, the symbol we use is _pointer to type id x_, where `x` is the ID of the
`MyStruct` type. This works the same for const types, array types, etc., While this means performing
more lookups overall to resolve a type like `const MyStruct*[3]`, it allows for lazily retrieving
the type (interning must be done eagerly to get all the base type IDs).

### Phase one: Public declaration collection

After we have a parsed AST, we begin by collecting all the function and struct declarations by their
semantic IDs. We do not resolve function bodies etc at this stage. Doing so would eliminate a later
pass, however, skipping this extra work allows us to do targeted compilation of certain files more
quickly.

Note that at this stage, we do create `ResolvedFunction`s and `ResolvedStruct`s, which describe the
public API, so a later pass for this information isn't necessary.

Struct information/IDs and function information/IDs are currently collected by different passes, but
these could be combined into a single pass for efficiency.

In the future, this could possibly be done at parse time to further improve compilation efficiency.

### Phase two: Imported ID collection

This stage is implemented through a couple queries.

Firstly, we collect the imports themselves (this could probably be combined with the declaration
collection pass for efficiency). This is the only AST pass in this stage.

Next, we expose a trimmed down version of the public API for each file, which only exposes the
semantic IDs of declarations defined in that file. This means updating a function or struct
signature does not invalidate this query result from the cache, so long as their names/IDs remain
unchanged.

Then we request these declaration ID for each import, and unify them to create the available symbol
table for the current file. Again, this doesn't contain the full details of the declarations, only
their semantic IDs.

### Phase three: Expression resolution

This phase translates the AST into a "resolved tree," which refers to the AST nodes but contains
resolution information like resolved types and references to declaration IDs.

Function references and type references are resolved to semantic IDs. We also create a registry for
local variables (including parameters) and assign these locally unique IDs.

This stage makes it faster to reanalyze a file after one of its imported symbols has changed.

### Phase four: Imported signature collection

This stage is very similar to phase two. However, where phase two collects _only_ the IDs of
imported declarations, this phase collects the full signatures of the imported declarations. It does
not involve any AST passes.

Note that the "full signatures" of an imported declaration still refers to other semantic IDs. For
example, `fun foo(x: int) -> MyStruct*` still refers to the semantic ID of types `int` and
`MyStruct`. If we actually exposed all of the information about `MyStruct` such as its members,
then this stage would be invalidated when the members of `MyStruct` change, even if `MyStruct` is
defined in a different file.

### Phase five: Type checking

Now that the full semantic information of all imported and locally defined declarations is
available, the type checking phase can proceed.

This step proceeds as one would expect.

#### A note on type resolution

Getting the definition of a function by its ID is simple; there should be one unique definition
associated with that ID, and that should be in a file explicitly imported by the current file.

However, types can be transitively referenced through multiple files, for instance, in the example
below, `main.rna` refers to `MyStruct` through an import chain.

```arena
// struct.rna
struct MyStruct { x: int }

// some_func.rna
import struct;
fun some_func() -> MyStruct* { ... }

// main.rna
import some_func;

fun main() {
    let x: int = some_func()..x;
}
```

This is not yet handled correctly, because we need to decide between a few options of how the
language should work:
1. We could transitively collect struct IDs until we reach a fixed point
2. This could resolve to an incomplete type unless `struct.rna` is imported directly by `main.rna`.
3. We could add the option (or _require_) that `some_func.rna` exports `MyStruct` via something like
   an `export` statement or a `public import` declaration.

Option 1 would be the most user-friendly, but would complicate and/or slow down compilation the
most. Option 3 would still require transitive import scanning, and any transitive scanning will
require cycle detection etc. Therefore, option 2 is still the simplest and fastest.

The current implementation has another, worse issue. Consider the following program, where multiple
files define a struct with the same name, and a function imported by `main.rna` refers to one of
them.

```arena
// a.rna
struct MyStruct { x: int }

// b.rna
struct MyStruct { x: long }

// some_func.rna
import a;
fun some_func() -> MyStruct* { ... }

// main.rna
import some_func;
import b;

fun main() {
    ...
    let x: long = some_func()..x;
    ...
}
```

At the moment this isn't handled correctly. The struct table for `main.rna` will contain the struct
from `b.rna`, and will not contain the one from `a.rna`. Assuming that `MyStruct` is interned to
an ID 8, then the function table will show that `some_func` returns a pointer to type 8. When we
look up type 8 in the type table for `main.rna`, we will get the definition from `b.rna`, not
`a.rna`.

This doesn't even lead to a linker error.

The fix is likely to require a project description that contains all source files in the project, so
that we can check each symbol is defined exactly once.

## Code generation

Currently, code generation is not performed in the query system.

In the future, performing code generation within the query system would allow for incremental
compilation. However, at the moment, we don't implement persistence to disk of query results. We
could implement this in the future, or implement a compilation server that maintains query results
in memory.

### Code generation performance opportunities

The static analysis described above is optimized for fast incremental analysis in an LSP context.
However, we could optimize code generation by combining passes slightly differently.

A code generation pass could potentially skip the "expression resolution" phase, and go straight to
typechecking. 

## Other known issues

The query system and semantic ID system do not currently have any kind of garbage collection.

In terms of the query system, this means that cached query results are never evicted, even if
unused. This can be done fairly straightforwardly. We can use the "verified revision" to see which
cached query results have recently been used, and evict old entries by some threshold.

In terms of semantic IDs, this means that if users create tons of function or struct names without
restarting the compiler, even if they are no longer used, memory usage will continue to grow
indefinitely. This is more difficult to handle compared to the query system, because interned names
are not easily evicted, and detecting unused interned names is non-trivial. We should implement some
form of stat collection to estimate when there are many unused interned names, and reassign semantic
IDs above a certain threshold. This will invalidate all analyses and therefore should be done as
rarely as possible.
