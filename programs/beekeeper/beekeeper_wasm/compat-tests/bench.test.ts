import { describe, test, beforeAll, afterAll } from 'vitest';
import { createOldBeekeeper, createNewBeekeeper, type ITestBeekeeper } from './helpers.js';
import { keys, signData } from './data.js';
import { existsSync, rmdirSync } from 'fs';
import { DEFAULT_STORAGE_ROOT as newRoot } from '@hiveio/beekeeper';
import { DEFAULT_STORAGE_ROOT as oldRoot } from '@hiveio/beekeeper-old';

const ITERATIONS = 10_000;

/** Run an async function N times and return total ms + ops/sec */
async function measure(name: string, iterations: number, fn: () => Promise<any>) {
  // Warmup
  for (let i = 0; i < 50; i++) await fn();

  const start = performance.now();
  for (let i = 0; i < iterations; i++) await fn();
  const elapsed = performance.now() - start;

  const opsPerSec = (iterations / elapsed) * 1000;
  console.log(`  ${name}: ${elapsed.toFixed(1)} ms total, ${opsPerSec.toFixed(1)} ops/sec (${iterations} iterations)`);
  return { elapsed, opsPerSec };
}

function printComparison(oldResult: { elapsed: number; opsPerSec: number }, newResult: { elapsed: number; opsPerSec: number }) {
  const ratio = oldResult.opsPerSec / newResult.opsPerSec;
  if (ratio > 1) {
    console.log(`  => old is ${ratio.toFixed(2)}x faster`);
  } else {
    console.log(`  => new is ${(1 / ratio).toFixed(2)}x faster`);
  }
  console.log();
}

describe('Benchmarks', () => {
  let oldBkSign: ITestBeekeeper;
  let newBkSign: ITestBeekeeper;
  let oldBkCrypt: ITestBeekeeper;
  let newBkCrypt: ITestBeekeeper;

  beforeAll(async () => {
    if (existsSync(oldRoot))
      rmdirSync(oldRoot, { recursive: true });
    if (newRoot !== oldRoot && existsSync(newRoot))
      rmdirSync(newRoot, { recursive: true });

    oldBkSign = await createOldBeekeeper([keys[3][0]]);
    newBkSign = await createNewBeekeeper([keys[3][0]]);
    oldBkCrypt = await createOldBeekeeper([keys[7][0]]);
    newBkCrypt = await createNewBeekeeper([keys[7][0]]);
  });

  afterAll(async () => {
    await oldBkSign.cleanup();
    await newBkSign.cleanup();
    await oldBkCrypt.cleanup();
    await newBkCrypt.cleanup();
  });

  test(`signDigest (${ITERATIONS} iterations)`, async () => {
    const sv = signData[0];
    console.log();
    const oldR = await measure('old', ITERATIONS, () => oldBkSign.signDigest(sv.publicKey, sv.sigDigest));
    const newR = await measure('new', ITERATIONS, () => newBkSign.signDigest(sv.publicKey, sv.sigDigest));
    printComparison(oldR, newR);
  }, 120_000);

  test(`encryptData (${ITERATIONS} iterations)`, async () => {
    const pubKey = keys[7][1];
    console.log();
    const oldR = await measure('old', ITERATIONS, () => oldBkCrypt.encryptData(pubKey, pubKey, 'benchmark payload'));
    const newR = await measure('new', ITERATIONS, () => newBkCrypt.encryptData(pubKey, pubKey, 'benchmark payload'));
    printComparison(oldR, newR);
  }, 120_000);

  test(`encrypt + decrypt roundtrip (${ITERATIONS} iterations)`, async () => {
    const pubKey = keys[7][1];
    console.log();
    const oldR = await measure('old', ITERATIONS, async () => {
      const cipher = await oldBkCrypt.encryptData(pubKey, pubKey, 'roundtrip payload');
      await oldBkCrypt.decryptData(pubKey, pubKey, cipher);
    });
    const newR = await measure('new', ITERATIONS, async () => {
      const cipher = await newBkCrypt.encryptData(pubKey, pubKey, 'roundtrip payload');
      await newBkCrypt.decryptData(pubKey, pubKey, cipher);
    });
    printComparison(oldR, newR);
  }, 120_000);

  test(`importKey (${ITERATIONS} iterations)`, async () => {
    let idx = 0;
    console.log();
    const oldR = await measure('old', ITERATIONS, () => {
      return oldBkSign.importKey(keys[idx++ % keys.length][0]);
    });
    const newR = await measure('new', ITERATIONS, () => {
      return newBkSign.importKey(keys[idx++ % keys.length][0]);
    });
    printComparison(oldR, newR);
  }, 120_000);
});
