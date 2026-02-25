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
    const retVal = await beekeeperTest.dynamic(async ({ beekeeper }) => {
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

      unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
      unlocked.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

      unlocked.removeKey('STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa');

      return unlocked.getPublicKeys();
    });

    expect(retVal).toStrictEqual(['STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh']);
  });

  test('Should be able to display only private keys in specific wallet', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked1 } = await session.createWallet('w0', 'mypassword');
      const { wallet: unlocked2 } = await session.createWallet('w1', 'mypassword');

      unlocked1.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
      unlocked2.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

      return unlocked1.getPublicKeys();
    });

    expect(retVal).toStrictEqual(['STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh']);
  });

  test('Should be able to create a wallet, import keys and check if matching key exists', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword');

      unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');

      return unlocked.hasMatchingPrivateKey('STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh');
    });

    expect(retVal).toBeTruthy();
  });

  test('Should not be able to import keys after closing a wallet', async ({ beekeeperTest }) => {
    await expect(beekeeperTest(async ({ beekeeper }) => {
      const session = beekeeper.createSession("my.salt");

      const { wallet: unlocked } = await session.createWallet('w0', 'mypassword');

      unlocked.close();

      unlocked.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT'); // This should fail
    })).rejects.toThrow(/Wallet not found: w0/);
  });

  test('Should be able to sign digest', async ({ beekeeperTest }) => {
    const retVal = await beekeeperTest(async ({ beekeeper }) => {
      const digestStr = "390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542";
      // Convert hex string to Uint8Array
      const uint8Array = new Uint8Array(digestStr.match(/.{1,2}/g)!.map(byte => parseInt(byte, 16)));

      const session = beekeeper.createSession("my.salt");

      const { wallet } = await session.createWallet('w0', 'mypassword');

      const publicKey = wallet.importKey('5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n');

      const signatureStr = wallet.signDigest(publicKey, digestStr);
      const signatureHex = wallet.signDigest(publicKey, uint8Array);

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

  test.afterAll(async () => {
    await browser.close();
  });
});
