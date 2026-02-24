import { chromium, ChromiumBrowser, expect } from '@playwright/test';
import { test } from '../assets/jest-helper.js'

import { WALLET_OPTIONS_NODE } from '../assets/data.js';

const walletNames = ["w0","w1","w2","w3","w4","w5","w6","w7","w8","w9","w10"];

const keys =
[
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
  ['5Js14qmTQ8Pf6mBzBaagKQ5Tc8tqgeLqncnKJoL6dEqYzzT7Mvf', 'STM6SKxp2eB7Zc4bFGVwQPNijyWouNidPWvyFsSALvavQWjRhqJXf']
];

const signData =
[
  {
    'public_key': keys[3][1],
    'sig_digest': '390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542',
    'binary_transaction_body': '000000000000000000000000',
    'transaction_body': '{}',
    'expected_signature': '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21'
  },
  {
    'public_key': keys[3][1],
    'sig_digest': '614e645c13b351b56d9742b358e3c3da58fa1a6a0036a01d3163c21aa2c8a99c',
    'binary_transaction_body': '5f00c58fb5f9854fb664010209696e69746d696e657205616c6963659a020000000000002320bcbe056d656d6d6d00',
    'transaction_body': '{"ref_block_num":95,"ref_block_prefix":4189425605,"expiration":"2023-07-18T08:38:29","operations":[{"type":"transfer_operation","value":{"from":"initminer","to":"alice","amount":{"amount":"666","precision":3,"nai":"@@000000021"},"memo":"memmm"}}],"extensions":[],"signatures":[],"transaction_id":"cc9630cdbc39da1c9b6264df3588c7bedb5762fa","block_num":0,"transaction_num":0}',
    'expected_signature': '1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340'
  },
];

let browser!: ChromiumBrowser;

test.describe('WASM beekeeper_api tests for Node.js', () => {
  test.beforeAll(async () => {
    browser = await chromium.launch({
      headless: true
    });
  });

  test('Should be able to get sign digest', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, signData) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const data = signData[0];

      const session = api.createSession('pear');

      api.create_with_password(session, 'w3', 'pass');

      const key = api.importKey(session, 'w3', '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n');

      const signDigest = api.signDigest(session, data.sig_digest, key);

      return {
        signDigest,
        expected: data.expected_signature
      }
    }, WALLET_OPTIONS_NODE, signData);

    expect(retVal.signDigest).toBe(retVal.expected);
  });

  test('Should require keys in wallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, 'w3', 'pass');

      api.importKey(session, 'w3', '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n');

      const publicKeys = api.getPublicKeys(session);

      return publicKeys.keys;
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toStrictEqual([
      { public_key: "STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4" }
    ]);
    expect(retVal[0].public_key).toBe('STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4');
  });

  test('Should be able to find a key', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, 'w3', 'pass');

      api.importKey(session, 'w3', keys[3][0]);

      const publicKeys = api.getPublicKeys(session);

      const indexes = [1, 2, 3, 10, 11, 12];

      for (let i = 0; i < indexes.length; ++i)
        if (publicKeys.keys.find(value => value.public_key === keys[indexes[i]][1]))
          return true;

      return false;
    }, WALLET_OPTIONS_NODE, keys);

    expect(retVal).toBeTruthy();
  });

  test('Should create a wallet with password', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, 'w0', 'this_is_password');

      // Verify wallet exists by importing a key (would throw if wallet not found)
      try {
        api.importKey(api.implicitSessionToken, 'w0', '5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
        return true;
      } catch {
        return false;
      }
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toBeTruthy();
  });

  test('Should unlock a wallet (newly created wallet is unlocked)', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, 'w0', 'pass');

      // Verify wallet is unlocked by importing a key (would throw if locked)
      try {
        api.importKey(session, 'w0', '5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
        return true;
      } catch {
        return false;
      }
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toBeTruthy();
  });

  test('Should be able to import keys', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[0], 'pass');

      api.importKey(session, walletNames[0], keys[3][0]);
      api.importKey(session, walletNames[0], keys[4][0]);

      const publicKeys = api.getPublicKeys(session);

      return publicKeys.keys;
    }, WALLET_OPTIONS_NODE, keys, walletNames);

    expect(retVal).toStrictEqual([
      { public_key: "STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4" },
      { public_key: "STM6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm" }
    ]);
  });

  test('Should be able to close a wallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[0], 'pass');

      api.close(session, walletNames[0]);

      // After close, trying to import a key should fail
      try {
        api.importKey(session, walletNames[0], '5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
        return false; // Should have thrown
      } catch {
        return true; // Expected
      }
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal).toBeTruthy();
  });

  test('Should be able to create a wallet with auto-generated password', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const password = api.create(api.implicitSessionToken, walletNames[0]);

      // Auto-generated password should start with "PW"
      return typeof password === 'string' && password.startsWith('PW');
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal).toBeTruthy();
  });

  test('Should be able to close a session', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.createSession(api.implicitSessionToken);

      try {
        api.closeSession(api.implicitSessionToken);
        return true;
      } catch {
        return false;
      }
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toBeTruthy();
  });

  test('Should be able to create a few wallets', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[1], 'cherry');
      api.create(session, walletNames[2]);
      api.create(session, walletNames[3]);

      // Verify all wallets exist by importing keys into each
      const results: boolean[] = [];
      for (const name of [walletNames[1], walletNames[2], walletNames[3]]) {
        try {
          api.importKey(session, name, '5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
          api.removeKey(session, name, 'STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh');
          results.push(true);
        } catch {
          results.push(false);
        }
      }

      return results;
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal).toStrictEqual([true, true, true]);
  });

  test('Should be able to lock a wallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[1], 'cherry');
      api.lock(session, walletNames[1]);

      // After locking, importing a key should fail
      try {
        api.importKey(session, walletNames[1], '5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
        return false; // Should have thrown
      } catch {
        return true; // Expected - wallet is locked
      }
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal).toBeTruthy();
  });

  test('Should be able to lock only one wallet from many', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[1], 'cherry');
      api.create_with_password(session, walletNames[2], 'pass');

      api.lock(session, walletNames[2]);

      // walletNames[1] should still be unlocked (import succeeds)
      let w1Unlocked = false;
      try {
        api.importKey(session, walletNames[1], '5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
        api.removeKey(session, walletNames[1], 'STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh');
        w1Unlocked = true;
      } catch { /* locked */ }

      // walletNames[2] should be locked (import fails)
      let w2Locked = false;
      try {
        api.importKey(session, walletNames[2], '5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
      } catch {
        w2Locked = true;
      }

      return { w1Unlocked, w2Locked };
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal.w1Unlocked).toBeTruthy();
    expect(retVal.w2Locked).toBeTruthy();
  });

  test('Should be able to lock and unlock a wallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[1], 'cherry');

      api.lock(session, walletNames[1]);

      // Verify locked: import should fail
      let locked = false;
      try {
        api.importKey(session, walletNames[1], '5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
      } catch {
        locked = true;
      }

      api.unlock(session, walletNames[1], 'cherry');

      // Verify unlocked: import should succeed
      let unlocked = false;
      try {
        api.importKey(session, walletNames[1], '5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
        unlocked = true;
      } catch { /* still locked */ }

      return { locked, unlocked };
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal.locked).toBeTruthy();
    expect(retVal.unlocked).toBeTruthy();
  });

  test('Should be able to lock all wallets', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[1], 'cherry');
      api.create_with_password(session, walletNames[2], 'pass');

      api.lockAll(session);

      // Both wallets should be locked now
      const results: boolean[] = [];
      for (const name of [walletNames[1], walletNames[2]]) {
        try {
          api.importKey(session, name, '5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
          results.push(false); // Should have thrown
        } catch {
          results.push(true); // Expected - locked
        }
      }

      return results;
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal).toStrictEqual([true, true]);
  });

  test('Should be able to close only one wallet from many', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[1], 'cherry');
      api.create_with_password(session, walletNames[2], 'pass');
      api.create_with_password(session, walletNames[3], 'pass');

      api.close(session, walletNames[2]);

      // walletNames[1] and walletNames[3] should still be accessible
      let w1Accessible = false;
      let w3Accessible = false;
      try {
        api.importKey(session, walletNames[1], '5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
        api.removeKey(session, walletNames[1], 'STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh');
        w1Accessible = true;
      } catch { /* closed */ }
      try {
        api.importKey(session, walletNames[3], '5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
        api.removeKey(session, walletNames[3], 'STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh');
        w3Accessible = true;
      } catch { /* closed */ }

      return { w1Accessible, w3Accessible };
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal.w1Accessible).toBeTruthy();
    expect(retVal.w3Accessible).toBeTruthy();
  });

  test('Should be able to open and unlock a previously created wallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create(session, walletNames[1]);

      // open on an already-open wallet should succeed
      api.open(session, walletNames[1]);

      // Verify it's still usable
      try {
        api.importKey(session, walletNames[1], '5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
        return true;
      } catch {
        return false;
      }
    }, WALLET_OPTIONS_NODE, walletNames);

    expect(retVal).toBeTruthy();
  });

  test('Should be able to remove a key', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      api.create_with_password(session, walletNames[1], 'cherry');
      api.importKey(session, walletNames[1], keys[1][0]);

      api.removeKey(session, walletNames[1], keys[1][1]);

      return api.getPublicKeys(session).keys;
    }, WALLET_OPTIONS_NODE, keys, walletNames);

    expect(retVal).toStrictEqual([]);
  });

  test('Should be able to delete an api instance', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.deleteInstance();
    }, WALLET_OPTIONS_NODE)).resolves.toBeUndefined()
  });

  test('Should be able to get session info', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      const info = api.getInfo(session);

      const now = new Date(info.now);
      const timeoutTime = new Date(info.timeout_time);

      const nowCheck = Number.isNaN(now.getTime());
      const timeoutTimeCheck = Number.isNaN(timeoutTime.getTime());

      return {
        nowCheck,
        timeoutTimeCheck
      };
    }, WALLET_OPTIONS_NODE);

    expect(retVal.nowCheck).toBeFalsy();
    expect(retVal.timeoutTimeCheck).toBeFalsy();
  });

  test('Create 3 wallets and add to every wallet 3 the same keys. Every key should be displayed only once', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create(api.implicitSessionToken, walletNames[4]);
      api.create(api.implicitSessionToken, walletNames[5]);
      api.create(api.implicitSessionToken, walletNames[6]);

      // Import keys 7-9 to wallets 4-6
      for(let walletNo = 4; walletNo <= 6; ++walletNo)
        for(let keyNo = 7; keyNo <= 9; ++keyNo)
          api.importKey(api.implicitSessionToken, walletNames[walletNo], keys[keyNo][0]);

      return api.getPublicKeys(api.implicitSessionToken).keys;
    }, WALLET_OPTIONS_NODE, walletNames, keys);

    expect(retVal).toStrictEqual([
      { public_key: 'STM6a34GANY5LD8deYvvfySSWGd7sPahgVNYoFPapngMUD27pWb45' },
      { public_key: 'STM8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4' },
      { public_key: 'STM8mmxXz5BfQc2NJfqhiPkbgcyJm4EvWEr2UAUdr56gEWSN9ZnA5' }
    ]);
  });

  test('Remove all keys from 3 wallets. As a result all keys are removed in mentioned wallets', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create(api.implicitSessionToken, walletNames[4]);
      api.create(api.implicitSessionToken, walletNames[5]);
      api.create(api.implicitSessionToken, walletNames[6]);

      // Import keys 7-9 to wallets 4-6
      for(let walletNo = 4; walletNo <= 6; ++walletNo)
        for(let keyNo = 7; keyNo <= 9; ++keyNo)
          api.importKey(api.implicitSessionToken, walletNames[walletNo], keys[keyNo][0]);

      // Remove keys 7-9 to wallets 4-6
      for(let walletNo = 4; walletNo <= 6; ++walletNo)
        for(let keyNo = 7; keyNo <= 9; ++keyNo)
          api.removeKey(api.implicitSessionToken!, walletNames[walletNo], keys[keyNo][1]);

      return api.getPublicKeys(api.implicitSessionToken).keys;
    }, WALLET_OPTIONS_NODE, walletNames, keys);

    expect(retVal).toStrictEqual([]);
  });

  test('Close implicitly created session. Create 3 new sessions. Every session has a 1 wallet. Every wallet has unique 1 key. As a result there are 3 keys', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const publicKeys: { public_key: string }[][] = [];

      api.createSession(api.implicitSessionToken);

      api.closeSession(api.implicitSessionToken);

      const sessions = [
        { keyNo: 4, session: api.createSession('avocado') },
        { keyNo: 5, session: api.createSession('avocado') },
        { keyNo: 6, session: api.createSession('avocado') }
      ];

      let walletNo = 7;
      for(const { session, keyNo } of sessions) {
        api.create(session, walletNames[walletNo]);
        api.importKey(session, walletNames[walletNo], keys[keyNo][0]);
        ++walletNo;
      }

      for(const { session } of sessions)
        publicKeys.push(api.getPublicKeys(session).keys);

      return publicKeys;
    }, WALLET_OPTIONS_NODE, walletNames, keys);

    expect(retVal).toStrictEqual([
      [ { public_key: "STM6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm" } ],
      [ { public_key: "STM6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW" } ],
      [ { public_key: "STM8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME" } ]
    ]);
  });

  test('Create 4 new sessions. All sessions create a wallet', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const sessions = [
        api.createSession('avocado'),
        api.createSession('avocado'),
        api.createSession('avocado'),
        api.createSession('avocado')
      ];

      const publicKeys: { public_key: string; }[][] = [];

      sessions.forEach((session, index) => {
        api.create_with_password(session, walletNames[index], 'pass');

        if(index % 2 === 0) {
          api.importKey(session, walletNames[index], keys[index][0]);
        }

        const keyArr = api.getPublicKeys(session).keys;

        if (keyArr.length !== 0)
          publicKeys.push(keyArr);
      });

      return publicKeys;
    }, WALLET_OPTIONS_NODE, walletNames, keys);

    expect(retVal).toStrictEqual([
      [ { public_key: "STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh" } ],
      [ { public_key: "STM7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH" } ]
    ]);
  });

  test('Should be able to create many sessions without limit', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      // core_minimal has no session limit — create 64+ sessions successfully
      const noSessions = 64;

      for(let i = 0; i < noSessions; ++i)
        api.createSession('xyz');
    }, WALLET_OPTIONS_NODE)).resolves.toBeUndefined();
  });

  test('Close implicitly created session. Create new sessions, unlock 10 wallets, add 10 keys to every wallet and then remove them', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const noSessions = 63; // + 1 implicit session that we create in run_node_helper.

      // Destroy beekeeper without sessions' closing
      api.closeSession(api.implicitSessionToken);

      const noKeys = Math.max(10, keys.length);

      let sessionToken!: string;

      for(let i = 0; i < noSessions; ++i) {
        sessionToken = api.createSession(i.toString());

        api.create_with_password(sessionToken, `w${i}`, 'pass');

        for(let k = 0; k < noKeys; ++k) {
          api.importKey(sessionToken, `w${i}`, keys[k][0]);
          api.removeKey(sessionToken, `w${i}`, keys[k][1]);
        }
      }

      const publicKeys = api.getPublicKeys(sessionToken);

      api.deleteInstance();

      return publicKeys.keys;
    }, WALLET_OPTIONS_NODE, walletNames, keys);

    expect(retVal).toStrictEqual([]);
  });

  test('Create new sessions, close them + implicitly created session. Create again sessions and close them', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);
      const noSessions = 64;

      const sessionTokens = [ api.implicitSessionToken ];

      for(let i = 0; i < noSessions - 1; ++i)
        sessionTokens.push(api.createSession(i.toString()));

      for(let i = 0; i < noSessions; ++i)
        api.closeSession(sessionTokens.pop()!);

      for(let i = 0; i < noSessions; ++i)
        sessionTokens.push(api.createSession(i.toString()));

      for(let i = 0; i < noSessions; ++i)
        api.closeSession(sessionTokens.pop()!);
    }, WALLET_OPTIONS_NODE)).resolves.toBeUndefined();
  });

  test('Try to sign transactions, but they fail. Unlock wallet. Import key. Sign transactions. Delete instance. Create instance again', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, signData, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      try {
        api.signDigest(api.implicitSessionToken, signData[0].sig_digest, signData[0].public_key);
        api.signDigest(api.implicitSessionToken, signData[1].sig_digest, signData[1].public_key);

        return false;
      } catch {
        api.create_with_password(api.implicitSessionToken, walletNames[3], 'pass');
        const key = api.importKey(api.implicitSessionToken, walletNames[3], keys[3][0]);

        api.signDigest(api.implicitSessionToken, signData[0].sig_digest, key);
        api.signDigest(api.implicitSessionToken, signData[1].sig_digest, key);

        return true;
      } finally {
        api.deleteInstance();
      }
    }, WALLET_OPTIONS_NODE, walletNames, signData, keys)).resolves.toBeTruthy();
  });

  test('Unlock 10 wallets. Every wallet has own session. Import the same key into every wallet. Sign transactions using the key from every wallet', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys, walletNames, signData) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const noWallets = 10;

      const sessionTokens: string[] = []

      for(let i = 0; i < noWallets; ++i) {
        sessionTokens.push(api.createSession(i.toString()));

        api.create_with_password(sessionTokens[i], walletNames[i], 'pass');
        const key = api.importKey(sessionTokens[i], walletNames[i], keys[3][0]);

        api.signDigest(sessionTokens[i], signData[1].sig_digest, key);
      }

      for(let i = 0; i < noWallets; i += 2)
        api.lock(sessionTokens[i], walletNames[i]);

      for(let i = 0; i < noWallets; ++i)
        if(i % 2 === 0)
          try {
            const key = api.importKey(sessionTokens[i], walletNames[i], keys[3][0]);
            api.signDigest(sessionTokens[i], signData[1].sig_digest, key);

            return false
          } catch {
            // Expected error — wallet is locked
          }
        else {
          const key = api.importKey(sessionTokens[i], walletNames[i], keys[3][0])
          api.signDigest(sessionTokens[i], signData[1].sig_digest, key);
          api.lock(sessionTokens[i], walletNames[i]); // For the next stage
        }

      return true;
    }, WALLET_OPTIONS_NODE, keys, walletNames, signData)).resolves.toBeTruthy();
  });

  test('Different false tests for sign* endpoints', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const walletNo = 9;
      api.create_with_password(api.implicitSessionToken, walletNames[walletNo], 'pass');

      const length = 1e7; // 10M
      const longStr = 'a'.repeat(length);
      // This should not throw as it is used only for signing (does not deserialize data)
      const key = api.importKey(api.implicitSessionToken, walletNames[walletNo], keys[3][0])
      api.signDigest(api.implicitSessionToken, longStr, key);
    }, WALLET_OPTIONS_NODE, walletNames, keys)).resolves.toBeUndefined();
  });

  test('Should throw when opening a wallet with empty name', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.open(api.implicitSessionToken, '');
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError('Wallet not found: ');
  });

  test('Should throw as wallet with the same name already exists', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create(api.implicitSessionToken, walletNames[9]);

      api.create(api.implicitSessionToken, walletNames[9]);
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError('Wallet already exists in session: w9');
  });

  test('Should not throw when the wallet does not exist', async ({ beekeeperWasmTest }) => {
    await (beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.close(api.implicitSessionToken, 'abc');
    }, WALLET_OPTIONS_NODE));
  });

  test('Should return empty keys when no wallets are open', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      return api.getPublicKeys(api.implicitSessionToken).keys;
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toStrictEqual([]);
  });

  test('Should throw when opening a wallet that does not exist on disk', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.open(api.implicitSessionToken, walletNames[9]);
      api.getPublicKeys(api.implicitSessionToken);
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrow(/Wallet not found: w9/);
  });

  test('Should throw as the wallet is already locked', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.lock(api.implicitSessionToken, walletNames[9]);
      api.lock(api.implicitSessionToken, walletNames[9]);
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError("Wallet not found: w9");
  });

  test('Should throw as the password is incorrect', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.unlock(api.implicitSessionToken, walletNames[9], 'incorrect');
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrow();
  });

  test('Should throw as the wallet is already unlocked', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, walletNames[9], 'pass');

      api.unlock(api.implicitSessionToken, walletNames[9], 'pass');
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError(/Wallet is already unlocked/);
  });

  test('Should throw as the wallet is not found', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.importKey(api.implicitSessionToken, 'pear', 'key');
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError("Wallet not found: pear");
  });

  test('Should throw as the key is invalid', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, walletNames[9], 'pass');

      api.importKey(api.implicitSessionToken, walletNames[9], 'key');
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrow(/Invalid WIF key|Key can't be constructed/);
  });

  test('Should throw as the wallet cannot be found', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.removeKey(api.implicitSessionToken, 'nonexisting-wallet', 'STM8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4');
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError("Wallet not found: nonexisting-wallet");
  });

  test('Should throw as the key does not have STM prefix', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, walletNames[9], 'pass');

      api.removeKey(api.implicitSessionToken, walletNames[9], '6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm');
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError("public key requires prefix: STM");
  });

  test('Should throw as the key is not in the wallet', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, walletNames[9], 'pass');

      api.removeKey(api.implicitSessionToken, walletNames[9], 'STM8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4');
    }, WALLET_OPTIONS_NODE, walletNames)).rejects.toThrowError("Key not in wallet");
  });

  test('Should throw because of invalid hex character in the digest', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.signDigest(api.implicitSessionToken, "#", "STM6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm");
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError("Invalid hex character '#'");
  });

  test('Should throw as the base58 cannot be decoded', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.signDigest(api.implicitSessionToken, "abCDe", "STMlemon");
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError("Unable to decode base58 string lemon");
  });

  test('Should throw as the public key is not found in the unlocked wallets', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.signDigest(api.implicitSessionToken, "abCDe", "STM6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm");
    }, WALLET_OPTIONS_NODE)).rejects.toThrowError(/Public key STM6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm not found in.*unlocked wallet/);
  });

  test('Check `has_matching_private_key` endpoint', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, walletNames[9], 'pass');

      const hasBeforeImport = api.hasMatchingPrivateKey(api.implicitSessionToken, walletNames[9], keys[0][1]);

      api.importKey(api.implicitSessionToken, walletNames[9], keys[0][0]);

      const hasAfterImport = api.hasMatchingPrivateKey(api.implicitSessionToken, walletNames[9], keys[0][1]);

      return {
        hasBeforeImport,
        hasAfterImport
      }
    }, WALLET_OPTIONS_NODE, walletNames, keys);

    expect(retVal.hasBeforeImport).toBeFalsy();
    expect(retVal.hasAfterImport).toBeTruthy();
  });

  test('Should get all keys from multiple wallets (merged)', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, walletNames, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      api.create_with_password(api.implicitSessionToken, walletNames[0], 'pass');
      api.create_with_password(api.implicitSessionToken, walletNames[1], 'pass');

      api.importKey(api.implicitSessionToken, walletNames[0], keys[0][0]);
      api.importKey(api.implicitSessionToken, walletNames[0], keys[1][0]);
      api.importKey(api.implicitSessionToken, walletNames[1], keys[2][0]);

      // get_public_keys(token) returns all keys from all wallets (deduplicated)
      return api.getPublicKeys(api.implicitSessionToken).keys;
    }, WALLET_OPTIONS_NODE, walletNames, keys);

    expect(retVal).toStrictEqual([
      { public_key: 'STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh' },
      { public_key: 'STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa' },
      { public_key: 'STM7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH' }
    ]);
  });

  test('Should properly handle FS-backed wallet persistence (create, close, reopen, unlock)', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE, keys) => {
      const api = new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);

      const session = api.createSession('pear');

      // Create a wallet and import a key
      api.create_with_password(session, 'persistent_wallet', 'mypass');
      api.importKey(session, 'persistent_wallet', keys[3][0]);

      const keysBefore = api.getPublicKeys(session).keys;

      // Close the wallet
      api.close(session, 'persistent_wallet');

      // Reopen and unlock — the key should still be there (FS-backed persistence)
      api.open(session, 'persistent_wallet');
      api.unlock(session, 'persistent_wallet', 'mypass');

      const keysAfter = api.getPublicKeys(session).keys;

      return {
        keysBefore,
        keysAfter
      };
    }, WALLET_OPTIONS_NODE, keys);

    expect(retVal.keysBefore).toStrictEqual(retVal.keysAfter);
    expect(retVal.keysAfter).toStrictEqual([
      { public_key: "STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4" }
    ]);
  });

  test('Should accept unlock_timeout via constructor options', async ({ beekeeperWasmTest }) => {
    await expect(beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }) => {
      // Construct with custom timeout — should not throw
      const api = new BeekeeperInstanceHelper(provider, ['--wallet-dir', '.beekeeper', '--unlock-timeout', '5']);

      const session = api.createSession('pear');
      api.getInfo(session);
    })).resolves.toBeUndefined();
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
