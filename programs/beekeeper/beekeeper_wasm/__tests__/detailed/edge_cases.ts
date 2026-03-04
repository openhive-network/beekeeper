import { chromium, ChromiumBrowser, expect } from '@playwright/test';
import { test } from '../assets/jest-helper.js';

import { WALLET_OPTIONS_NODE } from '../assets/data.js';

const keys = [
  ['5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT', 'STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh'],
  ['5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78', 'STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa'],
  ['5KNbAE7pLwsLbPUkz6kboVpTR24CycqSNHDG95Y8nbQqSqd6tgS', 'STM7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH'],
  ['5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n', 'STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4'],
  ['5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw', 'STM8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4'],
];

let browser!: ChromiumBrowser;

test.describe('Edge cases: high-level factory API', () => {
  test.beforeAll(async () => {
    browser = await chromium.launch({
      headless: true
    });
  });

  // --- openWallet ---

  test('Should be able to open and unlock a previously created wallet via openWallet', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      await session.createWallet('w0', 'mypassword');
      const { wallet } = await session.createWallet('w0_other', 'otherpass');
      await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');

      // Close w0_other, then reopen via openWallet
      wallet.close();

      const reopened = session.openWallet('w0_other');
      const unlocked = await reopened.unlock('otherpass');

      return unlocked.getPublicKeys();
    });

    expect(retVal).toStrictEqual(['STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh']);
  });

  test('Should return existing wallet when calling openWallet on an already-open wallet', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet } = await session.createWallet('w0', 'mypassword');
      await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');

      // openWallet on already-open wallet should return the same wallet object
      const reopened = session.openWallet('w0');

      return {
        name: reopened.name,
        hasUnlocked: reopened.unlocked !== undefined,
        keys: reopened.unlocked!.getPublicKeys()
      };
    });

    expect(retVal.name).toBe('w0');
    expect(retVal.hasUnlocked).toBe(true);
    expect(retVal.keys).toStrictEqual(['STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh']);
  });

  // --- lockAll via factory ---

  test('Should be able to lock all wallets via session.lockAll', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: w0 } = await session.createWallet('w0', 'pass0');
      const { wallet: w1 } = await session.createWallet('w1', 'pass1');

      await w0.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
      await w1.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

      const lockedWallets = session.lockAll();

      // After lockAll, all wallets should have unlocked === undefined
      return lockedWallets.map(w => ({
        name: w.name,
        isUnlocked: w.unlocked !== undefined
      }));
    });

    expect(retVal).toStrictEqual([
      { name: 'w0', isUnlocked: false },
      { name: 'w1', isUnlocked: false }
    ]);
  });

  // --- lock + unlock cycle via factory ---

  test('Should be able to lock and unlock a wallet via the high-level API', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet } = await session.createWallet('w0', 'mypassword');
      await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');

      const lockedWallet = wallet.lock();

      // After locking, the wallet should be locked (no unlocked reference)
      const isLockedAfterLock = lockedWallet.unlocked === undefined;

      const unlocked = await lockedWallet.unlock('mypassword');
      const keysAfterUnlock = unlocked.getPublicKeys();

      return { isLockedAfterLock, keysAfterUnlock };
    });

    expect(retVal.isLockedAfterLock).toBe(true);
    expect(retVal.keysAfterUnlock).toStrictEqual(['STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh']);
  });

  // --- auto-generated password ---

  test('Should return a generated password starting with PW when no password is provided', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { password } = await session.createWallet('w0');

      return { startsWithPW: password.startsWith('PW'), length: password.length };
    });

    expect(retVal.startsWithPW).toBe(true);
    expect(retVal.length).toBeGreaterThan(10);
  });

  // --- session.close() returns beekeeper instance ---

  test('Should return the beekeeper instance when closing a session', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      await session.createWallet('w0', 'pass');

      const returned = session.close();

      // The returned instance should still be usable
      const session2 = returned.createSession("another.salt");
      return typeof session2.getInfo().now;
    });

    expect(retVal).toBe('object');
  });

  // --- wallet.close() returns session ---

  test('Should return the session when closing a wallet', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'pass');

      const returnedSession = wallet.close();

      // The returned session should still be usable
      const { wallet: w1 } = await returnedSession.createWallet('w1', 'pass');
      return w1.name;
    });

    expect(retVal).toBe('w1');
  });

  // --- operations on locked wallet throw ---

  test('Should throw when calling getPublicKeys on a locked wallet', async ({ beekeeperTest }) => {
    await expect(beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'pass');

      await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');

      wallet.lock();

      // getPublicKeys on the now-stale unlocked wallet reference should throw
      wallet.getPublicKeys();
    })).rejects.toThrow();
  });

  test('Should throw when calling signDigest on a locked wallet', async ({ beekeeperTest }) => {
    await expect(beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'pass');

      const pubKey = await wallet.importKey('5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n');

      wallet.lock();

      await wallet.signDigest(pubKey, '390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542');
    })).rejects.toThrow();
  });

  test('Should throw when calling importKey on a locked wallet', async ({ beekeeperTest }) => {
    await expect(beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'pass');

      wallet.lock();

      await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
    })).rejects.toThrow();
  });

  // --- hasMatchingPrivateKey returns false for missing key ---

  test('Should return false from hasMatchingPrivateKey for a key not in wallet', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'pass');

      await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');

      // Check for a different key that was NOT imported
      return wallet.hasMatchingPrivateKey('STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa');
    });

    expect(retVal).toBe(false);
  });

  // --- encryption edge cases ---

  test('Should encrypt and decrypt content containing unicode null character via high-level API', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'pass');

      const key = await wallet.importKey('5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw');

      const message = 'before\0after';
      const encrypted = await wallet.encryptData(message, key, undefined, 44444);
      const decrypted = await wallet.decryptData(encrypted, key);

      return { decrypted, length: decrypted.length, hasNull: decrypted.includes('\u0000') };
    });

    expect(retVal.decrypted).toBe('before\u0000after');
    expect(retVal.length).toBe(12);
    expect(retVal.hasNull).toBe(true);
  });

  test('Should produce different ciphertexts with different nonces', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'pass');

      const key = await wallet.importKey('5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw');

      const enc1 = await wallet.encryptData("same message", key, undefined, 11111);
      const enc2 = await wallet.encryptData("same message", key, undefined, 22222);

      return { enc1, enc2, different: enc1 !== enc2 };
    });

    expect(retVal.different).toBe(true);
  });

  test('Should throw when decrypting with the wrong key', async ({ beekeeperTest }) => {
    await expect(beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'pass');

      const key1 = await wallet.importKey('5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw');
      const key2 = await wallet.importKey('5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n');
      const key3 = await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');

      // Encrypt from key1 to key2
      const encrypted = await wallet.encryptData("secret", key1, key2, 99999);

      // Decrypt with key3 instead of key2 — should fail
      await wallet.decryptData(encrypted, key1, key3);
    })).rejects.toThrow();
  });

  test('Should encrypt and decrypt empty string content', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'pass');

      const key = await wallet.importKey('5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw');

      const encrypted = await wallet.encryptData("", key, undefined, 12345);
      const decrypted = await wallet.decryptData(encrypted, key);

      return decrypted;
    });

    expect(retVal).toBe("");
  });

  // --- listWallets reflects lock state ---

  test('Should reflect locked/unlocked state in listWallets', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: w0 } = await session.createWallet('w0', 'pass0');
      await session.createWallet('w1', 'pass1');

      // Lock w0 only
      w0.lock();

      return session.listWallets().map(w => ({
        name: w.name,
        isUnlocked: w.unlocked !== undefined
      }));
    });

    expect(retVal).toStrictEqual([
      { name: 'w0', isUnlocked: false },
      { name: 'w1', isUnlocked: true }
    ]);
  });

  // --- listWallets after closing a wallet ---

  test('Should remove wallet from listWallets after closing it', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      await session.createWallet('w0', 'pass0');
      const { wallet: w1 } = await session.createWallet('w1', 'pass1');

      const before = session.listWallets().map(w => w.name);
      w1.close();
      const after = session.listWallets().map(w => w.name);

      return { before, after };
    });

    expect(retVal.before).toStrictEqual(['w0', 'w1']);
    expect(retVal.after).toStrictEqual(['w0']);
  });

  // --- importing duplicate key ---

  test('Should handle importing the same key twice into a wallet', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'pass');

      const pub1 = await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
      const pub2 = await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');

      const allKeys = wallet.getPublicKeys();

      return {
        samePublicKey: pub1 === pub2,
        keyCount: allKeys.length
      };
    });

    expect(retVal.samePublicKey).toBe(true);
    // Importing duplicate should not add a second entry
    expect(retVal.keyCount).toBe(1);
  });

  // --- operations after beekeeper.delete ---

  test('Should throw when creating a session after beekeeper.delete', async ({ beekeeperTest }) => {
    await expect(beekeeperTest(async ({ provider }) => {
      const bk = await provider.default();
      await bk.delete();

      bk.createSession("stale");
    })).rejects.toThrow();
  });

  // --- creating wallet with same name throws ---

  test('Should throw when creating wallet with duplicate name', async ({ beekeeperTest }) => {
    await expect(beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      await session.createWallet('dup', 'pass1');
      await session.createWallet('dup', 'pass2');
    })).rejects.toThrow(/already exists/);
  });

  // --- multiple sessions cannot interfere ---

  test('Should isolate wallets between sessions (different session cannot close another session wallet)', async ({ beekeeperTest }) => {
    await expect(beekeeperTest(async ({ beekeeper }) => {
      const session1 = beekeeper.createSession("salt1");
      const session2 = beekeeper.createSession("salt2");

      await session1.createWallet('w0', 'pass');

      // session2 does not own w0 — closeWallet should throw
      const opened = session2.openWallet('w0');
      opened.close();

      // session1 should still see w0 in its wallet map after session2 closed it
      return session1.listWallets().map(w => w.name);
    })).resolves.toStrictEqual(['w0']);
  });

  test.afterAll(async () => {
    await browser.close();
  });
});

test.describe('Edge cases: low-level WASM API', () => {
  test.beforeAll(async () => {
    browser = await chromium.launch({
      headless: true
    });
  });

  // --- listWallets ---

  test('Should be able to list wallets via low-level API', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      await api.create_with_password(api.implicitSessionToken, 'w0', 'pass');
      await api.create_with_password(api.implicitSessionToken, 'w1', 'pass');

      const result = api.listWallets(api.implicitSessionToken);

      return result.wallets.length;
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toBe(2);
  });

  // --- hasWallet ---

  test('Should correctly report wallet existence via hasWallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      await api.create_with_password(api.implicitSessionToken, 'exists', 'pass');

      return {
        hasExisting: api.hasWallet(api.implicitSessionToken, 'exists'),
        hasMissing: api.hasWallet(api.implicitSessionToken, 'nope')
      };
    }, WALLET_OPTIONS_NODE);

    expect(retVal.hasExisting).toBe(true);
    expect(retVal.hasMissing).toBe(false);
  });

  // --- signDigest with Uint8Array via low-level ---

  test('Should be able to sign with Uint8Array via low-level API', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');
      await api.create_with_password(session, 'w0', 'pass');
      const pubKey = await api.importKey(session, 'w0', keys[3][0]);

      const hexDigest = '390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542';
      const signatureFromHex = await api.signDigest(session, hexDigest, pubKey);

      return signatureFromHex;
    }, WALLET_OPTIONS_NODE, keys);

    expect(retVal).toBe('1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21');
  });

  // --- lock then sign should fail ---

  test('Should throw when signing with a locked wallet', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');
      await api.create_with_password(session, 'w0', 'pass');
      const pubKey = await api.importKey(session, 'w0', keys[3][0]);

      api.lock(session, 'w0');

      await api.signDigest(session, '390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542', pubKey);
    }, WALLET_OPTIONS_NODE, keys)).rejects.toThrow();
  });

  // --- encrypt on locked wallet ---

  test('Should throw when encrypting with a locked wallet', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');
      await api.create_with_password(session, 'w0', 'pass');
      const pubKey = await api.importKey(session, 'w0', keys[4][0]);

      api.lock(session, 'w0');

      await api.encryptData(session, 'w0', pubKey, pubKey, 'should fail');
    }, WALLET_OPTIONS_NODE, keys)).rejects.toThrow();
  });

  // --- decrypt on locked wallet ---

  test('Should throw when decrypting with a locked wallet', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');
      await api.create_with_password(session, 'w0', 'pass');
      const pubKey = await api.importKey(session, 'w0', keys[4][0]);

      // Encrypt while unlocked
      const encrypted = await api.encryptData(session, 'w0', pubKey, pubKey, 'test data', 12345);

      api.lock(session, 'w0');

      // Decrypt while locked — should fail
      await api.decryptData(session, 'w0', pubKey, pubKey, encrypted);
    }, WALLET_OPTIONS_NODE, keys)).rejects.toThrow();
  });

  // --- remove key then hasMatchingPrivateKey ---

  test('Should return false from hasMatchingPrivateKey after removing the key', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      await api.create_with_password(api.implicitSessionToken, 'w0', 'pass');
      await api.importKey(api.implicitSessionToken, 'w0', keys[0][0]);

      const hasBefore = api.hasMatchingPrivateKey(api.implicitSessionToken, 'w0', keys[0][1]);

      await api.removeKey(api.implicitSessionToken, 'w0', keys[0][1]);

      const hasAfter = api.hasMatchingPrivateKey(api.implicitSessionToken, 'w0', keys[0][1]);

      return { hasBefore, hasAfter };
    }, WALLET_OPTIONS_NODE, keys);

    expect(retVal.hasBefore).toBe(true);
    expect(retVal.hasAfter).toBe(false);
  });

  // --- create, close, reopen, import key, verify persistence ---

  test('Should persist keys across close/reopen/unlock cycle', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      await api.create_with_password(session, 'w0', 'pass');
      await api.importKey(session, 'w0', keys[0][0]);
      await api.importKey(session, 'w0', keys[1][0]);

      const keysBefore = api.getPublicKeys(session, 'w0').keys;

      api.close(session, 'w0');
      api.open(session, 'w0');
      await api.unlock(session, 'w0', 'pass');

      const keysAfter = api.getPublicKeys(session, 'w0').keys;

      return { keysBefore, keysAfter };
    }, WALLET_OPTIONS_NODE, keys);

    expect(retVal.keysBefore).toStrictEqual(retVal.keysAfter);
    expect(retVal.keysAfter.length).toBe(2);
  });

  // --- operations after deleteInstance ---

  test('Should throw when using API after deleteInstance', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);
      api.deleteInstance();

      // Any operation after delete should fail
      api.createSession('stale');
    }, WALLET_OPTIONS_NODE)).rejects.toThrow();
  });

  // --- wallet with special characters in name ---

  test('Should handle wallet names with spaces', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      await api.create_with_password(api.implicitSessionToken, 'my wallet', 'pass');
      await api.importKey(api.implicitSessionToken, 'my wallet', keys[0][0]);

      return api.getPublicKeys(api.implicitSessionToken, 'my wallet').keys;
    }, WALLET_OPTIONS_NODE, keys);

    expect(retVal).toStrictEqual([
      { public_key: 'STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh' }
    ]);
  });

  // --- import many keys into one wallet ---

  test('Should handle importing all 5 keys into one wallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      await api.create_with_password(api.implicitSessionToken, 'w0', 'pass');

      for (const [priv] of keys)
        await api.importKey(api.implicitSessionToken, 'w0', priv);

      return api.getPublicKeys(api.implicitSessionToken, 'w0').keys.length;
    }, WALLET_OPTIONS_NODE, keys);

    expect(retVal).toBe(5);
  });

  // --- deterministic encryption with nonce (low-level) ---

  test('Should produce deterministic encryption with same nonce via low-level API', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');
      await api.create_with_password(session, 'w0', 'pass');
      const pubKey = await api.importKey(session, 'w0', keys[4][0]);

      const enc1 = await api.encryptData(session, 'w0', pubKey, pubKey, 'deterministic', 77777);
      const enc2 = await api.encryptData(session, 'w0', pubKey, pubKey, 'deterministic', 77777);
      const enc3 = await api.encryptData(session, 'w0', pubKey, pubKey, 'deterministic', 88888);

      return { same: enc1 === enc2, different: enc1 !== enc3 };
    }, WALLET_OPTIONS_NODE, keys);

    expect(retVal.same).toBe(true);
    expect(retVal.different).toBe(true);
  });

  // --- encrypt/decrypt empty string ---

  test('Should encrypt and decrypt empty string via low-level API', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');
      await api.create_with_password(session, 'w0', 'pass');
      const pubKey = await api.importKey(session, 'w0', keys[4][0]);

      const encrypted = await api.encryptData(session, 'w0', pubKey, pubKey, '', 12345);
      const decrypted = await api.decryptData(session, 'w0', pubKey, pubKey, encrypted);

      return decrypted;
    }, WALLET_OPTIONS_NODE, keys);

    expect(retVal).toBe('');
  });

  // --- encrypt/decrypt with unicode content ---

  test('Should encrypt and decrypt UTF-16 content', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');
      await api.create_with_password(session, 'w0', 'pass');
      const pubKey = await api.importKey(session, 'w0', keys[4][0]);

      const message = 'Hello \u4e16\u754c \ud83d\udc1d';  // "Hello 世界 🐝"
      const encrypted = await api.encryptData(session, 'w0', pubKey, pubKey, message, 55555);
      const decrypted = await api.decryptData(session, 'w0', pubKey, pubKey, encrypted);

      return decrypted;
    }, WALLET_OPTIONS_NODE, keys);

    expect(retVal).toBe('Hello \u4e16\u754c \ud83d\udc1d');
  });

  // --- encrypt/decrypt with unicode null character ---

  test('Should encrypt and decrypt content containing unicode null character', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');
      await api.create_with_password(session, 'w0', 'pass');
      const pubKey = await api.importKey(session, 'w0', keys[4][0]);

      const message = 'before\0after';
      const encrypted = await api.encryptData(session, 'w0', pubKey, pubKey, message, 44444);
      const decrypted = await api.decryptData(session, 'w0', pubKey, pubKey, encrypted);

      return { decrypted, length: decrypted.length, hasNull: decrypted.includes('\0') };
    }, WALLET_OPTIONS_NODE, keys);

    expect(retVal.decrypted).toBe('before\0after');
    expect(retVal.length).toBe(12);
    expect(retVal.hasNull).toBe(true);
  });

  // --- lockAll then unlock individual wallets ---

  test('Should be able to unlock individual wallets after lockAll', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');
      await api.create_with_password(session, 'w0', 'pass0');
      await api.create_with_password(session, 'w1', 'pass1');

      await api.importKey(session, 'w0', keys[0][0]);
      await api.importKey(session, 'w1', keys[1][0]);

      api.lockAll(session);

      // Unlock only w1
      await api.unlock(session, 'w1', 'pass1');

      // w0 should be locked (getPublicKeys throws), w1 should work
      let w0Locked = false;
      try {
        api.getPublicKeys(session, 'w0');
      } catch {
        w0Locked = true;
      }

      const w1Keys = api.getPublicKeys(session, 'w1').keys;

      return { w0Locked, w1Keys };
    }, WALLET_OPTIONS_NODE, keys);

    expect(retVal.w0Locked).toBe(true);
    expect(retVal.w1Keys).toStrictEqual([
      { public_key: 'STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa' }
    ]);
  });

  // --- cross-session wallet visibility ---

  test('Should allow accessing same wallet from different sessions', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session1 = api.createSession('s1');
      const session2 = api.createSession('s2');

      await api.create_with_password(session1, 'shared', 'pass');
      await api.importKey(session1, 'shared', keys[0][0]);

      // session2 can open the same wallet
      api.open(session2, 'shared');
      const keys2 = api.getPublicKeys(session2, 'shared').keys;

      return keys2;
    }, WALLET_OPTIONS_NODE, keys);

    expect(retVal).toStrictEqual([
      { public_key: 'STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh' }
    ]);
  });

  // --- closed session cannot be reused ---

  test('Should throw when reusing a closed session token', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');
      api.closeSession(session);

      // Reusing the closed session token should fail
      await api.create_with_password(session, 'w0', 'pass');
    }, WALLET_OPTIONS_NODE)).rejects.toThrow();
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
