# Core host validation

## Behavioral mock tests

The runtime suite links production Core to deterministic mock parameter/module
callbacks and singleton state. Catch2 3.8.1 supplies assertions and CTest discovers
the ten cases. All pass on Apple Clang 21, arm64 macOS, C++23, including a seeded
random-order single-process run with 35 assertions. There are no sleeps, wall
clock reads, Arduino calls or attached-device requirements.

Configure from this library (with installed Base) or from the workspace:

```sh
cmake -S . -B build/host-mock -DGREVIR_BUILD_HOST_TESTS=ON \
  -DGREVIR_FETCH_TEST_DEPENDENCIES=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host-mock
ctest --test-dir build/host-mock --output-on-failure -L mock
```

For standalone Core, also supply `-DCMAKE_PREFIX_PATH=/your/grevir-install`.
Dependency downloading is allowed only by the explicit setup option above. The
archive is SHA-256 pinned. For offline setup, supply an installed Catch2 3.8.1
package or `-DGREVIR_CATCH2_SOURCE_DIR=/path/to/Catch2-3.8.1` and leave
`GREVIR_FETCH_TEST_DEPENDENCIES=OFF`. Test execution never downloads anything.
Catch2 and mock headers are not installed with Core.

Coverage: empty applications; parameter-before-module phases; dependency chains,
shared and uneven dependency graphs; duplicate roots and parameter dependencies;
state over repeated loops/setup; independent module identities; singleton identity
across translation units; and deterministic fixture reset. Initial runs found two
ordering failures; the dependency-order fix makes both regressions pass.

GPIO/time/register/interrupt behavior is not covered by these mock modules.
Hardware validation is on hold. Runtime test execution remains native only;
cross-compiler configurations are rejected when host tests are requested.

## Native compilation checks

Enable with `GREVIR_BUILD_COMPILE_CHECKS=ON`; `cmake --build` runs both targets:

- `grevir_core_compile`: every public header independently, concrete template
  users, historical dependency and resource-claim assertions, and adapted
  singleton module declarations. The singleton cases use compile-only pins and
  a sequence-poller stub, not the future behavioral hardware fixture. Empty
  historical runtime harness shells were not carried into production or tests.
- `grevir_core_claim_checks`: invokes the configured Clang/GNU compiler driver
  on `claim_cases.cpp`, without linking or executing an application. Two positive
  cases must pass before any expected failure is accepted. Each negative case
  must exit unsuccessfully and emit the expected static-assert diagnostic.

| Case | Expected result |
| --- | --- |
| 0 | Empty application compiles |
| 1 | Distinct pins, identical shared configurations, adjacent ranges and deduplicated dependencies compile |
| 2 | Exclusive pin claimed by different module types is rejected |
| 3 | Duplicate pin parameters in one module are rejected |
| 4 | Overlapping address ranges are rejected |
| 5 | Whole-resource and subrange claims are rejected |
| 6 | Different configurations of the same shared resource are rejected |
| 7 | Conflict introduced through a module dependency is rejected |
| 8 | Duplicate resources within one claim are rejected when compared with another parameter |
| 9 | Empty range is rejected when the range checker is instantiated |
| 10 | A two-module dependency cycle is rejected |
| 11 | A module depending on itself is rejected |

Outputs are retained under the build directory's `tests/claim-results/`.
These probes are currently for native Clang/GNU-style drivers; only Apple Clang
21 has been validated. They do not establish target compilation or runtime
behavior. Known untested/incorrect legacy cases are listed in the library README.

## Installed-package consumer

After separately installing Base and Core, build this consumer using only the
installation prefix:

```sh
cmake -S tests/installed-consumer -B build/consumer \
  -DCMAKE_CXX_COMPILER="$(xcrun --find clang++)" \
  -DCMAKE_PREFIX_PATH=/your/grevir-install
cmake --build build/consumer
```

The consumer has two translation units referencing the same singleton, two
distinct module types, application setup/loop calls and Base's diagnostic symbol.
The initial extraction checkpoint only linked this executable. The mock-validation
increment also ran it successfully using installed packages with source copies
hidden, establishing cross-file singleton identity in an installed consumer.
