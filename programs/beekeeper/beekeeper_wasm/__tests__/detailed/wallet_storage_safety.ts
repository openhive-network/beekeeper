import { expect } from '@playwright/test';
import { test } from '../assets/jest-helper.js';

/**
 * Wallet conversion and concurrency safety through the public TS API, on top of
 * storage callbacks the tests control (createBeekeeperWithStorage, see
 * __tests__/assets/globals.js).
 */

const FIXTURE_PASSWORD = 'fixture-pass';
const FIXTURE_PUBLIC_KEY = 'STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh';

// Same legacy fixture as in wallet_format.ts: pre-v1 scheme, password "fixture-pass", one key.
const LEGACY_WALLET_HEX =
  '23279e80bad106e06b60ee11322d79f92e2dd72eed60bcba837b05f90e50128ef0cf23d2ed81afcbee559a5c2acccd06' +
  '43e5a21d60ba61c7b2f6ed78a29e98403085382f536bf197c60af92641e71662eaf531e166ce07ba3fb1143d3bed942e' +
  'daaced3e1e6b30e5d5f33521b67cc58874cb617f6d9c7dd56caad67a10ef8940c500d816facf721cb8e610dd986d277e';

// Hex of "BEEKWLT" + version byte 0x01.
const V1_MAGIC_HEX = '4245454b574c5401';

const EXTRA_WIF_KEYS = [
  '5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78',
  '5KNbAE7pLwsLbPUkz6kboVpTR24CycqSNHDG95Y8nbQqSqd6tgS',
  '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n'
];

test.describe('Wallet conversion and concurrency', () => {
  test('A legacy wallet still unlocks when its converted copy cannot be saved', async ({ beekeeperWasmTest }) => {
    const result = await beekeeperWasmTest(async (_globals, legacyHex, password, magicHex) => {
      const store = new Map<string, Uint8Array>();
      store.set('legacy', new TextEncoder().encode('"' + legacyHex + '"'));
      let failSave = true;
      const storage = {
        save_fn: (name: string, data: Uint8Array) => {
          if (failSave)
            throw new Error('EACCES: permission denied');
          store.set(name, new Uint8Array(data));
        },
        load_fn: (name: string) => { const data = store.get(name); if (!data) throw new Error('Wallet not found: ' + name); return data; },
        list_dir_fn: () => Array.from(store.keys())
      };
      const isV1 = () => new TextDecoder().decode(store.get('legacy')).replace(/"/g, '').startsWith(magicHex);

      const bk = await createBeekeeperWithStorage(storage);
      const wallet = bk.createSession('salt').openWallet('legacy');
      const unlocked = await wallet.unlock(password);
      const keys = unlocked.getPublicKeys();
      const stillLegacy = !isV1();

      // Storage recovers: the next unlock converts the wallet.
      failSave = false;
      unlocked.lock();
      await wallet.unlock(password);
      const convertedAfterRecovery = isV1();

      await bk.delete();
      return { keys, stillLegacy, convertedAfterRecovery };
    }, LEGACY_WALLET_HEX, FIXTURE_PASSWORD, V1_MAGIC_HEX);

    expect(result.keys).toEqual([FIXTURE_PUBLIC_KEY]);
    expect(result.stillLegacy).toBe(true);
    expect(result.convertedAfterRecovery).toBe(true);
  });
});
