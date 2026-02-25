// @ts-expect-error ts(6133) Type BeekeeperError is used in JSDoc
import type { BeekeeperError } from "./errors";
import type { IStorageCallbacks } from "./fs";

export type TPublicKey = string;
export type TSignature = string;

export interface IWallet {
  /**
   * Ensures that this wallet is locked, then closes it
   *
   * @returns {IBeekeeperSession} Beekeeper session owning the closed wallet
   *
   * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  close(): IBeekeeperSession;

  /**
   * Name of this wallet
   *
   * @type {string}
   * @readonly
   */
  readonly name: string;
};

export interface IBeekeeperInfo {
  /**
   * Current server's time
   *
   * @type {Date}
   */
  now: Date;

  /**
   * Time when wallets will be automatically closed
   *
   * @type {Date}
   */
  timeoutTime: Date;
}

export interface IBeekeeperOptions {
  /**
   * @deprecated No longer used. Storage is controlled via the `storage` option.
   *
   * @type {string}
   */
  storageRoot: string;

  /**
   * The path to the WASM file. It can be a relative path or an absolute URL
   * If not specified, the default path is used: "./build/beekeeper_wasm.common.wasm" (may change if bundled)
   *
   * This option also accepts an inlined base64 encoded string of the WASM file as a value (`data:application/wasm;base64,...`)
   *
   * @type {string}
   */
  wasmLocation: string;

  /**
   * Whether internal Beekeeper core logs can be written. By default verbose Beekeeper core logs are disabled.
   *
   * Having this option disabled doesn't prevent you from seeing logs or catching detailed Beekeeper errors in your application.
   *
   * @default false
   */
  enableLogs: boolean;

  /**
   * Timeout for unlocked wallet in seconds (default 900 - 15 minutes).
   * Wallets will automatically lock after specified number of seconds of inactivity.
   * Activity is defined as any wallet command e.g. list-wallets
   *
   * @type {number}
   * @default 900
   */
  unlockTimeout: number;

  /**
   * Custom storage callbacks for wallet persistence.
   * If not provided, an in-memory Map-backed storage is used.
   *
   * @type {IStorageCallbacks}
   */
  storage?: IStorageCallbacks;
}

export interface IBeekeeperUnlockedWallet extends IWallet {
  /**
   * Locks the current wallet
   *
   * @returns {IBeekeeperWallet} Locked beekeeper wallet
   *
   * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  lock(): IBeekeeperWallet;

  /**
   * Imports given private key to this wallet and returns the associated public key for further use
   *
   * @param {string} wifKey private key in WIF format to import
   *
   * @returns {TPublicKey} Public key associated with the imported private key in WIF format with 'STM' prefix
   *
   * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  importKey(wifKey: string): TPublicKey;

  /**
   * Removes given key from this wallet
   *
   * @param {TPublicKey} publicKey public key in WIF format to match the private key in the wallet to remove
   *
   * @throws {BeekeeperError} if key not found or on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  removeKey(publicKey: TPublicKey): void;

  /**
   * Checks if this wallet has a private key matching the given public key
   *
   * @param {TPublicKey} publicKey public key in WIF format to match the private key in the wallet
   *
   * @returns {boolean} `true` if a matching private key is found otherwise `false`
   *
   * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  hasMatchingPrivateKey(publicKey: TPublicKey): boolean;

  /**
   * Signs a transaction by signing a digest of the transaction
   *
   * @param {TPublicKey} publicKey public key in WIF format to match the private key in the wallet. It will be used to sign the provided data
   * @param {string | Uint8Array} sigDigest digest of a transaction in hex format or as an array of bytes to be signed
   *
   * @returns {TSignature} signed data in hex format
   *
   * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  signDigest(publicKey: TPublicKey, sigDigest: string | Uint8Array): TSignature;

  /**
   * Lists all of the public keys inside this wallet
   *
   * @returns {TPublicKey[]} a set of all keys for this wallet
   *
   * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  getPublicKeys(): TPublicKey[];
}

export interface IBeekeeperWallet extends IWallet {
  /**
   * Unlocks this wallet
   *
   * @param {string} password password to the wallet
   *
   * @returns {IBeekeeperUnlockedWallet} Unlocked Beekeeper wallet
   *
   * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  unlock(password: string): IBeekeeperUnlockedWallet;

  /**
   * Indicates if the wallet is unlocked. If the wallet is locked, this property will be undefined. IBeekeeperUnlockedWallet type otherwise
   *
   * @type {?IBeekeeperUnlockedWallet}
   * @readonly
   */
  readonly unlocked?: IBeekeeperUnlockedWallet;
};

export interface IWalletCreated {
  /**
   * Unlocked, ready to use wallet
   *
   * @type {IBeekeeperUnlockedWallet}
   */
  wallet: IBeekeeperUnlockedWallet;

  /**
   * Password used for unlocking your wallet
   *
   * @type {string}
   */
  password: string;
}

export interface IBeekeeperSession {
  /**
   * Retrieves the current session info
   *
   * @returns {IBeekeeperInfo} Current session information
   *
   * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  getInfo(): IBeekeeperInfo;

  /**
   * Creates a new Beekeeper wallet object owned by this session.
   *
   * @param {string} name name of wallet
   * @param {?string} password password used for creation of a wallet. Should be strong enough to protect your keys.
   *                           If not provided, safe password is automatically generated and returned.
   *
   * @returns {Promise<IWalletCreated>} the created unlocked Beekeeper wallet object
   *
   * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  createWallet(name: string, password?: string): Promise<IWalletCreated>;

  /**
   * Opens Beekeeper wallet object owned by this session
   *
   * @param {string} name name of wallet
   *
   * @returns {IBeekeeperWallet} the opened Beekeeper wallet object (may be unlocked if it has been previously unlocked)
   *
   * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  openWallet(name: string): IBeekeeperWallet;

  /**
   * Locks all of the unlocked wallets owned by this session
   *
   * @returns {Array<IBeekeeperWallet>} array of the locked Beekeeper wallets
   *
   * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  lockAll(): Array<IBeekeeperWallet>;

  /**
   * Locks all of the unlocked wallets, closes them, closes this session and makes it unusable
   *
   * @returns {IBeekeeperInstance} Beekeeper instance owning the closed session
   *
   * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  close(): IBeekeeperInstance;
}

export interface IBeekeeperInstance {
  /**
   * Creation of a session
   *
   * @param {string} salt a salt used for creation of a token. It can be any string but should be random and unique per session
   *                      (for example: crypto.randomUUID() or Math.random())
   *
   * @returns {IBeekeeperSession} a beekeeper session created explicitly. It can be used for further work for example: creating/closing wallets, importing keys, signing transactions etc.
   *
   * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  createSession(salt: string): IBeekeeperSession;

  /**
   * Retrieves the app build version of the Beekeeper API in SemVer format
   *
   * @returns {string} the version of the Beekeeper API
   */
  getVersion(): string;

  /**
   * Locks all of the unlocked wallets, closes them, closes opened sessions and deletes the current Beekeeper API instance making it unusable
   *
   * @throws {BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error etc.)
   */
  delete(): Promise<void>;
}
