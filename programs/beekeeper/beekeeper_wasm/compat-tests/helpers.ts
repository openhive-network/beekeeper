import type { IBeekeeperInstance, IBeekeeperUnlockedWallet } from '@hiveio/beekeeper';

/** Uniform interface for both old and new beekeeper versions */
export interface ITestBeekeeper {
  signDigest(pubKey: string, digest: string): Promise<string>;
  encryptData(fromKey: string, toKey: string, content: string, nonce?: number): Promise<string>;
  decryptData(fromKey: string, toKey: string, content: string): Promise<string>;
  importKey(wifKey: string): Promise<string>;
  getPublicKeys(): Promise<string[]>;
  cleanup(): Promise<void>;
}

/**
 * Creates a new-version beekeeper instance with a wallet and pre-imported keys.
 * All methods are natively async in the new version.
 */
export async function createNewBeekeeper(wifKeys: string[]): Promise<ITestBeekeeper> {
  const createBeekeeper = (await import('@hiveio/beekeeper')).default;
  const bk = await createBeekeeper({ inMemory: true });
  const session = bk.createSession('compat-test-new');
  const { wallet } = await session.createWallet('w0', 'password');

  for (const wif of wifKeys)
    await wallet.importKey(wif);

  return {
    signDigest: (pubKey, digest) => wallet.signDigest(pubKey, digest),
    encryptData: (from, to, content, nonce?) =>
      wallet.encryptData(content, from, to, nonce),
    decryptData: (from, to, content) =>
      wallet.decryptData(content, from, to),
    importKey: (wif) => wallet.importKey(wif),
    getPublicKeys: () => wallet.getPublicKeys(),
    cleanup: () => bk.delete(),
  };
}

/**
 * Creates an old-version (1.28.7-rc0) beekeeper instance.
 * Most methods are synchronous in the old version — we wrap them in Promises.
 */
export async function createOldBeekeeper(wifKeys: string[]): Promise<ITestBeekeeper> {
  // The old package is aliased as @hiveio/beekeeper-old in package.json
  const mod = await import('@hiveio/beekeeper-old');
  const createBeekeeper = mod.default as unknown as (options?: Record<string, unknown>) => Promise<IBeekeeperInstance>;
  const bk = await createBeekeeper({ inMemory: true });
  const session = bk.createSession('compat-test-old');
  const { wallet } = await session.createWallet('w0', 'password');

  // In the old version, most wallet methods are synchronous
  const oldWallet = wallet as unknown as IOldUnlockedWallet;

  for (const wif of wifKeys)
    await wallet.importKey(wif);

  return {
    signDigest: async (pubKey, digest) => oldWallet.signDigest(pubKey, digest),
    encryptData: async (from, to, content, nonce?) =>
      nonce !== undefined
        ? oldWallet.encryptData(content, from, to, nonce)
        : oldWallet.encryptData(content, from, to),
    decryptData: async (from, to, content) => oldWallet.decryptData(content, from, to),
    importKey: (wif) => wallet.importKey(wif),
    getPublicKeys: async () => oldWallet.getPublicKeys(),
    cleanup: () => bk.delete(),
  };
}

/** Old API shape — sync methods that existed in 1.28.7-rc0 */
interface IOldUnlockedWallet {
  signDigest(publicKey: string, sigDigest: string | Uint8Array): string;
  encryptData(content: string, key: string, anotherKey?: string, nonce?: number): string;
  decryptData(content: string, key: string, anotherKey?: string): string;
  getPublicKeys(): string[];
}
