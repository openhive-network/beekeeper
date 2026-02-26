import { beekeeper_api, MainModule } from "../../dist/build/beekeeper_wasm.common.js";
import type { ICryptoCallbacks } from "../../src/detailed/crypto.js";

export declare class ExtractError extends Error {
  constructor(parsed: object);

  public parsed: object;
}

export interface IStorageFns {
  save_fn: (name: string, data: Uint8Array) => void;
  load_fn: (name: string) => Uint8Array;
  list_dir_fn: () => string[];
}

export interface IBeekeeperInstanceHelperConstructorSimplified {
  new(options: string[], storageFns?: IStorageFns): BeekeeperInstanceHelper;
}

export declare class BeekeeperInstanceHelper {
  public get implicitSessionToken(): string;

  public get instance(): beekeeper_api;

  public set setAcceptError(acceptError: boolean);

  public static cryptoCallbacks: ICryptoCallbacks | undefined;

  public static createInMemoryStorage(): IStorageFns & { store: Map<string, Uint8Array> };

  public static for(provider: MainModule): IBeekeeperInstanceHelperConstructorSimplified;

  public constructor(provider: MainModule, options: string[], storageFns?: IStorageFns);

  public createSession(salt: string): string;

  public createSessionWithoutSalt(): string;

  public closeSession(token: string): string;

  public hasMatchingPrivateKey(token: string, walletName: string, publicKey: string): boolean;

  public hasWallet(token: string, walletName: string): boolean;

  public listWallets(token: string): { wallets: Array<{ name: string; unlocked: boolean }> };

  public create(sessionToken: string, walletName: string): Promise<string>;

  public create_with_password(sessionToken: string, walletName: string, explicitPassword: string): Promise<string>;

  public importKey(sessionToken: string, walletName: string, key: string): Promise<string>;

  public removeKey(sessionToken: string, walletName: string, key: string): Promise<string>;

  public encryptData(sessionToken: string, walletName: string, fromPublicKey: string, toPublicKey: string, content: string, nonce?: number): Promise<string>;

  public decryptData(sessionToken: string, walletName: string, fromPublicKey: string, toPublicKey: string, encryptedContent: string): Promise<string>;

  public signDigest(sessionToken: string, sigDigest: string, publicKey: string): string;

  public getPublicKeys(sessionToken: string): { keys: Array<{ public_key: string }> };

  public getInfo(sessionToken: string): { now: string, timeout_time: string };

  public open(sessionToken: string, walletName: string): string;

  public close(sessionToken: string, walletName: string): string;

  public unlock(sessionToken: string, walletName: string, explicitPassword: string): Promise<string>;

  public lock(sessionToken: string, walletName: string): Promise<string>;

  public lockAll(sessionToken: string): Promise<string>;

  public deleteInstance(): void;
}
