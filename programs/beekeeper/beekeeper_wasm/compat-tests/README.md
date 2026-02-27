# Compatibility tests after optimization

```bash
# Ensure that the tests are run with the latest build of the main package
cd ../ && pnpm build && cd compat-tests

# Install dependencies and run tests
pnpm install --ignore-workspace

# Run the tests
pnpm test

# Run benchmarks
pnpm bench
```

## Current status

```text
stdout | bench.test.ts > Benchmarks > signDigest (10000 iterations)
  old: 2577.6 ms total, 3879.5 ops/sec (10000 iterations)
  new: 2240.6 ms total, 4463.1 ops/sec (10000 iterations)
  => new is 1.15x faster

stdout | bench.test.ts > Benchmarks > encryptData (10000 iterations)
  old: 2503.5 ms total, 3994.3 ops/sec (10000 iterations)
  new: 3714.2 ms total, 2692.4 ops/sec (10000 iterations)
  => old is 1.48x faster

stdout | bench.test.ts > Benchmarks > encrypt + decrypt roundtrip (10000 iterations)
  old: 5077.3 ms total, 1969.6 ops/sec (10000 iterations)
  new: 7045.4 ms total, 1419.4 ops/sec (10000 iterations)
  => old is 1.39x faster

stdout | bench.test.ts > Benchmarks > importKey (10000 iterations)
  old: 2301.8 ms total, 4344.5 ops/sec (10000 iterations)
  new: 2769.3 ms total, 3611.0 ops/sec (10000 iterations)
  => old is 1.20x faster
```
