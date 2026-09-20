# Grevir Core

Portable module parameters/dependencies, application composition and lifecycle,
singleton storage, resource claims/checks, board inventory traits and resource
topology extracted from Ardoinus. This header-only library depends only on Grevir
Base. Include `<GrevirCore.h>` or a `<grevir/core/...hpp>` header. Existing `ardo`
and `setl` API names remain; the resource graph also retains its historical
`ardo::sys::avr::base` namespace despite having no AVR or register-access dependency.

This is a local development snapshot, not a published release. GPIO, clocks,
Arduino services, board selection and MMIO access are not part of Core.

## Native build

Install Grevir Base first, then configure Core against that prefix:

```sh
cmake -S . -B build/native -G "Unix Makefiles" \
  -DCMAKE_CXX_COMPILER="$(xcrun --find clang++)" \
  -DCMAKE_PREFIX_PATH=/your/grevir-install \
  -DGREVIR_BUILD_COMPILE_CHECKS=ON
cmake --build build/native
cmake --install build/native --prefix /your/grevir-install
```

Consumers use `find_package(grevir-core CONFIG REQUIRED)` and link `grevir::core`.
Base is found as an installed dependency or supplied as an existing CMake target;
Core never searches sibling checkouts. Normal library-only configuration leaves
compile checks disabled and does not download dependencies.

For Arduino packaging, both checkouts belong directly under `libraries/`.
`library.properties` declares `depends=Grevir Base`. Real Arduino dependency
discovery and target toolchains remain unvalidated.

## Host mock validation — 20 September 2026

Ten Catch2/CTest tests now execute production Core code with deterministic mock
modules. They cover empty applications, parameter/module callback phases,
dependency chains and shared dependencies, duplicate roots, parameter-supplied
dependencies, persistent and independent state, singleton identity across source
files, and fixture reset. All pass on native Apple Clang 21, including a seeded
random-order run of all tests in one process (35 assertions).

The tests exposed a legacy ordering bug: reversing the dependency closure could
run a consumer before its shared dependency. `lifecycle_order.hpp` now computes
a dependency-first callback order while preserving `AllModules` membership and
the reverse ordering of independent modules. Each parameter phase still runs
before the corresponding module phase. Ordering guarantees apply within each
phase: parameter callbacks do not run after dependency module setup. Shared
dependency modules run once per phase. Cyclic module dependencies, including
self-cycles, now fail compilation with `GREVIR_CORE_DEPENDENCY_CYCLE`.

Enable runtime tests with `GREVIR_BUILD_HOST_TESTS=ON`. Catch2 3.8.1 is an opt-in
host development dependency; normal library builds and installed consumers do not
require it. See [tests/README.md](tests/README.md) for online/offline setup and
execution commands. Tests also pass in a standalone Core checkout with installed
Base and a locally supplied Catch2 source. The installed consumer now runs too.

Hardware validation is on hold. These fixtures validate Core dispatch and storage;
they do not validate GPIO, elapsed time, interrupts, register side effects or MCU
backend behavior. Those behavioral fixtures remain future host-side work.

## Compilation and limits

Apple Clang 21 on arm64 macOS, C++23, standard library enabled:

- All eleven public headers compile independently. Concrete application lifecycle,
  closure, graph lookup, board traits and singleton uses compile.
- The historical dependency assertions and shared/range claim assertions compile.
  Both historical singleton module styles compile against test-only bindings.
- Two valid application probes compile. Eight conflict probes and two cycle probes
  are rejected with their expected diagnostics; see [tests/README.md](tests/README.md).
- Separate copies of Base and Core build and install outside the workspace. A
  two-translation-unit consumer links against installed packages with both source
  copies hidden, resolving singleton storage and Base's compiled diagnostic hook.

The compilation checks remain separately available without a test runner. Neither
compilation nor runtime checks require Arduino, a target compiler or an MCU fixture.

The initial relocation preserved algorithms and diagnostics; the mock-validation
increment deliberately fixes callback ordering as described above. The graph's dependency on the old
register header was removed by using Base's equivalent `tuple_contained_in`
trait in place of `has_type_v`. `device_map.hpp` now directly includes its required
integer declarations. Production headers do not include test-only bindings.

Parameter indexing was fixed on 21 September 2026: `Parameters<...>::Param<N>`
now resolves to the selected type through recursive inheritance and a zero-index
specialization. Regression assertions cover first/middle/last indices, repeated
types, references and incomplete types. Empty lists and out-of-range indices are
rejected with `GREVIR_CORE_PARAMETER_INDEX_OUT_OF_RANGE`. The public alias is unchanged.

Known inherited limitations, explicitly deferred to focused correctness changes:

- A single module with a single parameter containing duplicate resources can
  evade the internal-claim check. The negative internal-claim probe includes a
  second parameter to exercise the existing checker; it does not establish full
  coverage of every application shape.
- `RootDependencies<T>` returns an empty tuple for a resource whose own
  `dependencies` is empty, although nested traversal treats that resource as a
  root. This legacy distinction is preserved and covered by static assertions.
- `allocation.hpp` contains only the legacy `SelectionResolver` pass-through
  placeholder. It is not a resource allocator. Resource graph cycles beyond the
  documented self-root convention are unsupported; use an acyclic graph.

The original source remains in Ardoinus. Source history import, API namespace
modernization and eventual removal from the original checkout are separate work.
The original project's MIT notice is copied unchanged in `LICENSE.txt`.
