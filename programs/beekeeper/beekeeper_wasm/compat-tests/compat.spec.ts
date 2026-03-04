/**
 * Compatibility tests between old (1.28.7-rc0) and new beekeeper.
 * Each test runs in both Node.js and Chromium via the compatTest fixture.
 */
import { existsSync, rmSync } from 'fs';
import { test, expect } from './compat-fixture.js';
import { DEFAULT_STORAGE_ROOT as newRoot } from '@hiveio/beekeeper';
import { DEFAULT_STORAGE_ROOT as oldRoot } from '@hiveio/beekeeper-old';

const keys: [string, string][] = [
  ['5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT', 'STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh'],
  ['5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78', 'STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa'],
  ['5KNbAE7pLwsLbPUkz6kboVpTR24CycqSNHDG95Y8nbQqSqd6tgS', 'STM7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH'],
  ['5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n', 'STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4'],
  ['5J8C7BMfvMFXFkvPhHNk2NHGk4zy3jF4Mrpf5k5EzAecuuzqDnn', 'STM6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm'],
  ['5J15npVK6qABGsbdsLnJdaF5esrEWxeejeE3KUx6r534ug4tyze', 'STM6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW'],
  ['5K1gv5rEtHiACVTFq9ikhEijezMh4rkbbTPqu4CAGMnXcTLC1su', 'STM8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME'],
  ['5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw', 'STM8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4'],
  ['5KXNQP5feaaXpp28yRrGaFeNYZT7Vrb1PqLEyo7E3pJiG1veLKG', 'STM6a34GANY5LD8deYvvfySSWGd7sPahgVNYoFPapngMUD27pWb45'],
  ['5KKvoNaCPtN9vUEU1Zq9epSAVsEPEtocbJsp7pjZndt9Rn4dNRg', 'STM8mmxXz5BfQc2NJfqhiPkbgcyJm4EvWEr2UAUdr56gEWSN9ZnA5'],
  ['5Jen4tBsMEDyr4iNfDUTiUXbdrZZ7BuFa5oVHxBT7Zybm71Fmjz', 'STM7FMWqA7f5oYov4pmhfUDd4JJENRguZ4Sv7d3i2jt6Rz2Sg37fh'],
  ['5Jo6ALLFrTBK9pbzdNYKxZt6wPfKtRa9bXrek7Ypo988PKJZWgV', 'STM6iEHkB8ohBUCGWfftcEWmNyRtqZhu4m8sbc5c2QYv2AWuMHt5k'],
  ['5Js14qmTQ8Pf6mBzBaagKQ5Tc8tqgeLqncnKJoL6dEqYzzT7Mvf', 'STM6SKxp2eB7Zc4bFGVwQPNijyWouNidPWvyFsSALvavQWjRhqJXf'],
];

const signData = [
  {
    publicKey: keys[3][1],
    sigDigest: '390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542',
    expectedSignature: '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21',
  },
  {
    publicKey: keys[3][1],
    sigDigest: '614e645c13b351b56d9742b358e3c3da58fa1a6a0036a01d3163c21aa2c8a99c',
    expectedSignature: '1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340',
  },
];

test.describe('Signature compatibility', () => {
  test.beforeAll(() => {
    if (existsSync(oldRoot))
      rmSync(oldRoot, { recursive: true });
    if (newRoot !== oldRoot && existsSync(newRoot))
      rmSync(newRoot, { recursive: true });
  });

  test('old and new produce identical signatures', async ({ compatTest }) => {
    const result = await compatTest(async ({ createOld, createNew }, keys, signData) => {
      const oldBk = await createOld({ inMemory: true });
      const newBk = await createNew({ inMemory: true });

      const oldSession = oldBk.createSession('compat-old');
      const newSession = newBk.createSession('compat-new');

      const { wallet: oldW } = await oldSession.createWallet('w0', 'password');
      const { wallet: newW } = await newSession.createWallet('w0', 'password');

      await oldW.importKey(keys[3][0]);
      await newW.importKey(keys[3][0]);

      const results = [];
      for (const sv of signData) {
        const oldSig = await oldW.signDigest(sv.publicKey, sv.sigDigest);
        const newSig = await newW.signDigest(sv.publicKey, sv.sigDigest);
        results.push({ oldSig, newSig, expected: sv.expectedSignature });
      }

      await oldBk.delete();
      await newBk.delete();

      return results;
    }, keys, signData);

    for (const r of result) {
      expect(r.oldSig).toBe(r.expected);
      expect(r.newSig).toBe(r.expected);
      expect(r.oldSig).toBe(r.newSig);
    }
  });
});

test.describe('Key import compatibility', () => {
  test('all 13 keys produce identical public keys', async ({ compatTest }) => {
    const result = await compatTest(async ({ createOld, createNew }, keys) => {
      const oldBk = await createOld({ inMemory: true });
      const newBk = await createNew({ inMemory: true });

      const oldSession = oldBk.createSession('compat-old');
      const newSession = newBk.createSession('compat-new');

      const { wallet: oldW } = await oldSession.createWallet('w0', 'password');
      const { wallet: newW } = await newSession.createWallet('w0', 'password');

      for (const [wif] of keys) {
        await oldW.importKey(wif);
        await newW.importKey(wif);
      }

      const oldPubKeys = oldW.getPublicKeys().sort();
      const newPubKeys = newW.getPublicKeys().sort();

      await oldBk.delete();
      await newBk.delete();

      return { oldPubKeys, newPubKeys, expectedPubKeys: keys.map((k: any) => k[1]).sort() };
    }, keys);

    expect(result.oldPubKeys).toEqual(result.expectedPubKeys);
    expect(result.newPubKeys).toEqual(result.expectedPubKeys);
    expect(result.oldPubKeys).toEqual(result.newPubKeys);
  });
});

test.describe('Encryption compatibility (same key)', () => {
  test('deterministic encryption: identical ciphertext with same nonce', async ({ compatTest }) => {
    const result = await compatTest(async ({ createOld, createNew }, keys) => {
      const oldBk = await createOld({ inMemory: true });
      const newBk = await createNew({ inMemory: true });

      const oldSession = oldBk.createSession('compat-old');
      const newSession = newBk.createSession('compat-new');

      const { wallet: oldW } = await oldSession.createWallet('w0', 'password');
      const { wallet: newW } = await newSession.createWallet('w0', 'password');

      const pubKey = keys[7][1];
      await oldW.importKey(keys[7][0]);
      await newW.importKey(keys[7][0]);

      const oldCipher = await oldW.encryptData('hello beekeeper', pubKey, undefined, 12345);
      const newCipher = await newW.encryptData('hello beekeeper', pubKey, undefined, 12345);

      await oldBk.delete();
      await newBk.delete();

      return { oldCipher, newCipher };
    }, keys);

    expect(result.oldCipher).toBe(result.newCipher);
  });

  test('cross-version decryption: new decrypts old, old decrypts new', async ({ compatTest }) => {
    const result = await compatTest(async ({ createOld, createNew }, keys) => {
      const oldBk = await createOld({ inMemory: true });
      const newBk = await createNew({ inMemory: true });

      const oldSession = oldBk.createSession('compat-old');
      const newSession = newBk.createSession('compat-new');

      const { wallet: oldW } = await oldSession.createWallet('w0', 'password');
      const { wallet: newW } = await newSession.createWallet('w0', 'password');

      const pubKey = keys[7][1];
      await oldW.importKey(keys[7][0]);
      await newW.importKey(keys[7][0]);

      const message = 'cross-version test';

      // Old encrypt → new decrypt
      const oldCipher = await oldW.encryptData(message, pubKey, undefined, 99999);
      const newDecrypted = await newW.decryptData(oldCipher, pubKey);

      // New encrypt → old decrypt
      const newCipher = await newW.encryptData(message, pubKey, undefined, 88888);
      const oldDecrypted = await oldW.decryptData(newCipher, pubKey);

      await oldBk.delete();
      await newBk.delete();

      return { message, newDecrypted, oldDecrypted };
    }, keys);

    expect(result.newDecrypted).toBe(result.message);
    expect(result.oldDecrypted).toBe(result.message);
  });

  test('repeated calls with same nonce give same output', async ({ compatTest }) => {
    const result = await compatTest(async ({ createOld, createNew }, keys) => {
      const oldBk = await createOld({ inMemory: true });
      const newBk = await createNew({ inMemory: true });

      const oldSession = oldBk.createSession('compat-old');
      const newSession = newBk.createSession('compat-new');

      const { wallet: oldW } = await oldSession.createWallet('w0', 'password');
      const { wallet: newW } = await newSession.createWallet('w0', 'password');

      const pubKey = keys[7][1];
      await oldW.importKey(keys[7][0]);
      await newW.importKey(keys[7][0]);

      const old1 = await oldW.encryptData('deterministic test', pubKey, undefined, 99999);
      const old2 = await oldW.encryptData('deterministic test', pubKey, undefined, 99999);
      const new1 = await newW.encryptData('deterministic test', pubKey, undefined, 99999);
      const new2 = await newW.encryptData('deterministic test', pubKey, undefined, 99999);

      await oldBk.delete();
      await newBk.delete();

      return { old1, old2, new1, new2 };
    }, keys);

    expect(result.old1).toBe(result.old2);
    expect(result.new1).toBe(result.new2);
    expect(result.old1).toBe(result.new1);
  });
});

test.describe('Encryption compatibility (cross-key)', () => {
  test('cross-key: identical ciphertext with same nonce', async ({ compatTest }) => {
    const result = await compatTest(async ({ createOld, createNew }, keys) => {
      const oldBk = await createOld({ inMemory: true });
      const newBk = await createNew({ inMemory: true });

      const oldSession = oldBk.createSession('compat-old');
      const newSession = newBk.createSession('compat-new');

      const { wallet: oldW } = await oldSession.createWallet('w0', 'password');
      const { wallet: newW } = await newSession.createWallet('w0', 'password');

      const fromPub = keys[7][1];
      const toPub = keys[3][1];
      await oldW.importKey(keys[7][0]);
      await oldW.importKey(keys[3][0]);
      await newW.importKey(keys[7][0]);
      await newW.importKey(keys[3][0]);

      const oldCipher = await oldW.encryptData('cross-key msg', fromPub, toPub, 54321);
      const newCipher = await newW.encryptData('cross-key msg', fromPub, toPub, 54321);

      await oldBk.delete();
      await newBk.delete();

      return { oldCipher, newCipher };
    }, keys);

    expect(result.oldCipher).toBe(result.newCipher);
  });

  test('cross-key: cross-version decryption', async ({ compatTest }) => {
    const result = await compatTest(async ({ createOld, createNew }, keys) => {
      const oldBk = await createOld({ inMemory: true });
      const newBk = await createNew({ inMemory: true });

      const oldSession = oldBk.createSession('compat-old');
      const newSession = newBk.createSession('compat-new');

      const { wallet: oldW } = await oldSession.createWallet('w0', 'password');
      const { wallet: newW } = await newSession.createWallet('w0', 'password');

      const fromPub = keys[7][1];
      const toPub = keys[3][1];
      await oldW.importKey(keys[7][0]);
      await oldW.importKey(keys[3][0]);
      await newW.importKey(keys[7][0]);
      await newW.importKey(keys[3][0]);

      const message = 'cross-key cross-version';

      const oldCipher = await oldW.encryptData(message, fromPub, toPub, 54321);
      const newDecrypted = await newW.decryptData(oldCipher, fromPub, toPub);

      const newCipher = await newW.encryptData(message, fromPub, toPub, 12345);
      const oldDecrypted = await oldW.decryptData(newCipher, fromPub, toPub);

      await oldBk.delete();
      await newBk.delete();

      return { message, newDecrypted, oldDecrypted };
    }, keys);

    expect(result.newDecrypted).toBe(result.message);
    expect(result.oldDecrypted).toBe(result.message);
  });
});

test.describe('Wallet compatibility (cross-version)', () => {
  test('persistent wallet: old creates, new reopens', async ({ compatTest }) => {
    const result = await compatTest(async ({ createOld, createNew }, keys) => {
      // Intentionally do not create with inMemory!!!
      const oldBk = await createOld();

      const oldSession = oldBk.createSession('compat-old');
      const { wallet: oldW } = await oldSession.createWallet('w0', 'password');

      const fromPub = keys[7][1];
      const toPub = keys[3][1];
      await oldW.importKey(keys[7][0]);
      await oldW.importKey(keys[3][0]);

      const message = 'cross-version wallet test';

      const oldCipher = await oldW.encryptData(message, fromPub, toPub, 54321);

      // Close all the wallets and sessions
      await oldBk.delete();

      // Re-open with new beekeeper, which should read the same wallet data - after migration
      const newBk = await createNew();
      const newSession = newBk.createSession('compat-new');
      // Re-open the same wallet and unlock it
      const newW = await newSession.openWallet('w0').unlock('password');
      const newCipher = await newW.encryptData(message, fromPub, toPub, 54321);

      await newBk.delete();

      return { oldCipher, newCipher };
    }, keys);

    expect(result.oldCipher).toBe(result.newCipher);
  });
});
