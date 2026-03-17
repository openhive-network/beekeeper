import { chromium, ChromiumBrowser, expect } from '@playwright/test';
import { test } from '../assets/jest-helper.js';

let browser!: ChromiumBrowser;

test.describe('Beekeeper factory tests for Node.js', () => {
  test.beforeAll(async () => {
    browser = await chromium.launch({
      headless: true
    });
  });

  test('Should be able to retrieve package version', async ({ beekeeperTest }) => {
    const version = await beekeeperTest(({ beekeeper }) => {
      return beekeeper.getVersion();
    });

    expect(typeof version).toBe('string');
  });

  test('Should be able to init the beekeeper factory', async ({ beekeeperTest }) => {
    await beekeeperTest(async ({ beekeeper }) => {
      beekeeper.createSession("my.salt");

      await beekeeper.delete();
    });
  });

  test('Should be able to get_info based on the created session', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest.dynamic(({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      return session.getInfo();
    });

    expect(retVal).toHaveProperty('now');
    expect(retVal).toHaveProperty('timeoutTime');

    expect(retVal.now).toBeInstanceOf(Date);
    expect(retVal.timeoutTime).toBeInstanceOf(Date);
  });

  test('Should be able to create multiple sessions with wallets and delete the beekeeper instance', async ({ beekeeperTest }) => {
    await expect(beekeeperTest(async ({ beekeeper }) => {
      const session1 = beekeeper.createSession("avocado1");
      const session2 = beekeeper.createSession("avocado2");

      await session1.createWallet('w0');
      await session2.createWallet('w1');

      await beekeeper.delete();
    })).resolves.toBeUndefined();
  });

  test('Should be able to create a wallet and import and remove keys', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword');

      await unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
      await unlocked.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

      await unlocked.removeKey('STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa');

      return unlocked.getPublicKeys();
    });

    expect(retVal).toStrictEqual(['STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh']);
  });

  test('Should be able to display only private keys in specific wallet', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked1 } = await session.createWallet('w0', 'mypassword');
      const { wallet: unlocked2 } = await session.createWallet('w1', 'mypassword');

      await unlocked1.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
      await unlocked2.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

      return unlocked1.getPublicKeys();
    });

    expect(retVal).toStrictEqual(['STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh']);
  });

  test('Should be able to create a wallet, import keys and check if matching key exists', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword');

      await unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');

      return unlocked.hasMatchingPrivateKey('STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh');
    });

    expect(retVal).toBeTruthy();
  });

  test('Should not be able to import keys after closing a wallet', async ({ beekeeperTest }) => {
    await expect(beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword');

      unlocked.close();

      await unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT'); // This should fail
    })).rejects.toThrow(/Wallet not found: w0/);
  });

  test('Should be able to do a single inMemory sign', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ provider }) => {
      const bk = await provider.default({ inMemory: true });

      const session = bk.createSession("my.salt");
      const { wallet } = await session.createWallet('tmp', 'mypassword');

      const publicKey = await wallet.importKey('5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n');

      const signature = await wallet.signDigest(publicKey, "390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542");

      await bk.delete();

      return signature;
    });

    expect(retVal).toBe("1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21");
  });

  test('Should wallet be a temporary wallet when inMemory is true', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ provider }) => {
      const bk = await provider.default({ inMemory: true });
      const session = bk.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword');

      return unlocked.isTemporary;
    });

    expect(retVal).toBeTruthy();
  });

  test('Should wallet not be temporary by default', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword');

      return unlocked.isTemporary;
    });

    expect(retVal).toBeFalsy();
  });

  test('Should be able to create an explicitly temporary wallet', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword', true);

      return unlocked.isTemporary;
    });

    expect(retVal).toBeTruthy();
  });

  test('Should have a wallet via hasWallet after creation', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      await session.createWallet('w0', 'mypassword');

      return session.hasWallet('w0');
    });

    expect(retVal).toBeTruthy();
  });

  test('Should not have a non-existent wallet via hasWallet', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      return session.hasWallet('nonexistent');
    });

    expect(retVal).toBeFalsy();
  });

  test('Should be able to create multiple wallets and access them using listWallets references', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      await session.createWallet('w0', 'mypassword');
      await session.createWallet('w1', 'mypassword');
      await session.createWallet('w2', 'mypassword');

      return session.listWallets().map(value => value.name);
    });

    expect(retVal).toStrictEqual(['w0','w1','w2']);
  });

  test('Should be able to encrypt and decrypt data (round-trip)', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'mypassword');

      const key = await wallet.importKey('5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw');

      const encrypted = await wallet.encryptData("hello beekeeper", key, undefined, 12345);
      const decrypted = await wallet.decryptData(encrypted, key);

      return { encrypted, decrypted };
    });

    expect(retVal.decrypted).toBe("hello beekeeper");
    expect(typeof retVal.encrypted).toBe('string');
    expect(retVal.encrypted.length).toBeGreaterThan(0);
  });

  test('Should be able to encrypt for another key and decrypt with that key', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'mypassword');

      const key1 = await wallet.importKey('5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw');
      const key2 = await wallet.importKey('5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n');

      // Encrypt from key1 to key2
      const encrypted = await wallet.encryptData("secret message", key1, key2, 67890);
      // Decrypt using key2 (receiver) and key1 (sender)
      const decrypted = await wallet.decryptData(encrypted, key1, key2);

      return decrypted;
    });

    expect(retVal).toBe("secret message");
  });

  test('Should produce deterministic encryption with explicit nonce', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'mypassword');

      const key = await wallet.importKey('5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw');

      const encrypted1 = await wallet.encryptData("deterministic test", key, undefined, 99999);
      const encrypted2 = await wallet.encryptData("deterministic test", key, undefined, 99999);

      return { encrypted1, encrypted2 };
    });

    expect(retVal.encrypted1).toBe(retVal.encrypted2);
  });

  test('Should be able to create a beekeeper instance with custom unlockTimeout', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest.dynamic(async ({ provider }) => {
      const bk = await provider.default({ unlockTimeout: 5 });

      const session = bk.createSession("my.salt");
      const info = session.getInfo();

      await bk.delete();

      return info;
    });

    expect(retVal).toHaveProperty('now');
    expect(retVal).toHaveProperty('timeoutTime');
    expect(retVal.now).toBeInstanceOf(Date);
    expect(retVal.timeoutTime).toBeInstanceOf(Date);
  });

  test('Should be able to sign digest', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const digestStr = "390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542";
      // Convert hex string to Uint8Array
      const uint8Array = new Uint8Array(digestStr.match(/.{1,2}/g)!.map(byte => parseInt(byte, 16)));

      const session = beekeeper.createSession("my.salt");

      const { wallet } = await session.createWallet('w0', 'mypassword');

      const publicKey = await wallet.importKey('5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n');

      const signatureStr = await wallet.signDigest(publicKey, digestStr);
      const signatureHex = await wallet.signDigest(publicKey, uint8Array);

      return {
        fromString: signatureStr,
        fromHex: signatureHex
      };
    });

    const expected = "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21";

    expect(retVal).toStrictEqual({
      fromString: expected,
      fromHex: expected
    });
  });

  test('Should return timeout_time in the future', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest.dynamic(({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      const info = session.getInfo();

      return {
        nowMs: info.now.getTime(),
        timeoutMs: info.timeoutTime.getTime()
      };
    });

    expect(retVal.timeoutMs).toBeGreaterThan(retVal.nowMs);
  });

  test('Should return timeout_time reflecting custom unlockTimeout', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest.dynamic(async ({ provider }) => {
      const bk = await provider.default({ unlockTimeout: 10 });
      const session = bk.createSession("my.salt");
      const info = session.getInfo();

      const diffSeconds = (info.timeoutTime.getTime() - info.now.getTime()) / 1000;

      await bk.delete();

      return diffSeconds;
    });

    // Should be roughly 10 seconds (allow ±2s tolerance for execution time)
    expect(retVal).toBeGreaterThanOrEqual(8);
    expect(retVal).toBeLessThanOrEqual(12);
  });

  test('Should auto-lock wallets after timeout expires', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest.dynamic(async ({ provider }) => {
      const bk = await provider.default({ unlockTimeout: 2 });
      const session = bk.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'mypassword');

      await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');

      const keysBefore = wallet.getPublicKeys();

      // Wait for timeout to expire
      await new Promise(resolve => setTimeout(resolve, 3000));

      // After timeout, accessing wallet.unlocked auto-locks and returns undefined
      const walletRef = session.listWallets()[0];
      const isLocked = walletRef.unlocked === undefined;

      await bk.delete();

      return {
        keysBefore: keysBefore.length,
        isLocked
      };
    });

    expect(retVal.keysBefore).toBe(1);
    expect(retVal.isLocked).toBe(true);
  });

  test('Should refresh timeout on wallet operations', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest.dynamic(async ({ provider }) => {
      const bk = await provider.default({ unlockTimeout: 5 });
      const session = bk.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'mypassword');

      await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');

      // Wait 3 seconds (within 5s timeout)
      await new Promise(resolve => setTimeout(resolve, 3000));

      // Perform an operation to refresh timeout
      await wallet.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

      // Wait another 3 seconds (6s total, but only 3s since last operation)
      await new Promise(resolve => setTimeout(resolve, 3000));

      // Wallet should still be unlocked since timeout was refreshed
      const walletRef = session.listWallets()[0];
      const keys = walletRef.unlocked?.getPublicKeys();

      await bk.delete();

      return keys?.length;
    });

    expect(retVal).toBe(2);
  });

  test('Should throw when using a closed session', async ({ beekeeperTest }) => {
    await expect(beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");
      session.close();

      // Operations on a closed session should throw
      await session.createWallet('w0', 'mypassword');
    })).rejects.toThrow();
  });

  test('Should throw timeout error when using stale unlocked wallet reference', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest.dynamic(async ({ provider }) => {
      const bk = await provider.default({ unlockTimeout: 2 });
      const session = bk.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'mypassword');

      await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');

      // Hold a direct reference to the unlocked wallet
      const staleRef = wallet;

      // Wait for timeout to expire
      await new Promise(resolve => setTimeout(resolve, 3000));

      // Using the stale reference should throw a timeout error
      let threwTimeout = false;
      try {
        staleRef.getPublicKeys();
      } catch (error) {
        threwTimeout = String(error).toLowerCase().includes('timeout');
      }

      await bk.delete();

      return threwTimeout;
    });

    expect(retVal).toBe(true);
  });

  test('Should not throw timeout error on unlock (unlock refreshes timeout)', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest.dynamic(async ({ provider }) => {
      const bk = await provider.default({ unlockTimeout: 2 });
      const session = bk.createSession("my.salt");
      const { wallet } = await session.createWallet('w0', 'mypassword');

      await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
      wallet.lock();

      // Wait for timeout to expire
      await new Promise(resolve => setTimeout(resolve, 3000));

      // Unlock should not throw — it refreshes the timeout
      const walletRef = session.listWallets()[0];
      const unlocked = await walletRef.unlock('mypassword');
      const keys = unlocked.getPublicKeys();

      await bk.delete();

      return keys.length;
    });

    expect(retVal).toBe(1);
  });

  test('Should not throw timeout error on hasWallet after timeout', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest.dynamic(async ({ provider }) => {
      const bk = await provider.default({ unlockTimeout: 2 });
      const session = bk.createSession("my.salt");
      await session.createWallet('w0', 'mypassword');

      // Wait for timeout to expire
      await new Promise(resolve => setTimeout(resolve, 3000));

      // hasWallet should work regardless of timeout state
      const exists = session.hasWallet('w0');
      const notExists = session.hasWallet('nonexistent');

      await bk.delete();

      return { exists, notExists };
    });

    expect(retVal.exists).toBe(true);
    expect(retVal.notExists).toBe(false);
  });

  test('Should not throw timeout error on lockAll after timeout', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest.dynamic(async ({ provider }) => {
      const bk = await provider.default({ unlockTimeout: 2 });
      const session = bk.createSession("my.salt");
      await session.createWallet('w0', 'mypassword');

      // Wait for timeout to expire
      await new Promise(resolve => setTimeout(resolve, 3000));

      // lockAll should work without throwing timeout error
      const wallets = session.lockAll();

      await bk.delete();

      return wallets.length;
    });

    expect(retVal).toBe(1);
  });

  test('Should not throw timeout error on createSession after timeout', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest.dynamic(async ({ provider }) => {
      const bk = await provider.default({ unlockTimeout: 2 });
      const session1 = bk.createSession("salt1");
      await session1.createWallet('w0', 'mypassword');

      // Wait for timeout to expire
      await new Promise(resolve => setTimeout(resolve, 3000));

      // Creating a new session should not throw timeout error
      const session2 = bk.createSession("salt2");

      await bk.delete();

      return session2 !== undefined;
    });

    expect(retVal).toBe(true);
  });

  test('Should not throw timeout error on delete after timeout', async ({ beekeeperTest }) => {
    await beekeeperTest.dynamic(async ({ provider }) => {
      const bk = await provider.default({ unlockTimeout: 2 });
      const session = bk.createSession("my.salt");
      await session.createWallet('w0', 'mypassword');

      // Wait for timeout to expire
      await new Promise(resolve => setTimeout(resolve, 3000));

      // delete should work without throwing timeout error
      await bk.delete();

      return true;
    });
  });

  // --- wallet persistence across re-initialization ---
  // All persistence assertions run in a single beekeeperTest.dynamic call so
  // the clean/setup logic is defined once and closures serialize correctly
  // for the browser eval context.

  test('Should persist wallets across re-initialization (listCreatedWallets, hasWallet, listWallets, keys)', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest.dynamic(async ({ provider, env }) => {
      const root = './bk_persist_test';

      const clean = async () => {
        if (env === 'web') {
          await new Promise<void>(r => { const req = indexedDB.deleteDatabase(root); req.onsuccess = req.onerror = () => r(); });
        } else {
          const m = 'fs'; (await import(m) as any).rmSync(root, { recursive: true, force: true });
        }
      };

      await clean();

      try {
        // First instance: create a wallet with a key and persist it
        const bk1 = await provider.default({ storageRoot: root });
        const s1 = bk1.createSession("salt1");
        const { wallet } = await s1.createWallet('w0', 'mypassword');
        await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
        s1.close();
        await bk1.delete();

        // Second instance: same storage, fresh session
        const bk2 = await provider.default({ storageRoot: root });
        const s2 = bk2.createSession("salt2");

        // listCreatedWallets must enumerate wallets in storage as wallet objects
        const createdWallets = s2.listCreatedWallets();
        const createdNames = createdWallets.map((w: any) => w.name);

        // listWallets must return only opened wallets (none yet)
        const openedBefore = s2.listWallets().map((w: any) => w.name);

        // hasWallet must detect persisted wallets
        const hasExisting = s2.hasWallet('w0');
        const hasMissing = s2.hasWallet('never_created');

        // unlock via wallet object from listCreatedWallets (lazy-opens in C++)
        const w0 = createdWallets.find((w: any) => w.name === 'w0')!;
        const unlocked = await w0.unlock('mypassword');
        const keys = unlocked.getPublicKeys();

        // after lazy-open via unlock, listWallets must include it
        const openedAfter = s2.listWallets().map((w: any) => w.name);

        await bk2.delete();

        return { createdNames, openedBefore, openedAfter, hasExisting, hasMissing, keys };
      } finally {
        await clean();
      }
    });

    expect(retVal.createdNames).toContain('w0');
    expect(retVal.openedBefore).not.toContain('w0');
    expect(retVal.openedAfter).toContain('w0');
    expect(retVal.hasExisting).toBe(true);
    expect(retVal.hasMissing).toBe(false);
    expect(retVal.keys).toStrictEqual(['STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh']);
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
