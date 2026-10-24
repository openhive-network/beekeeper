import type { MainModule, beekeeper_api } from "../build/beekeeper_wasm.common";

import { BeekeeperError } from "./errors.js";
import type { ICryptoCallbacks } from "./crypto.js";
import type { IStorageCallbacks } from "./fs.js";
import { IBeekeeperInstance, IBeekeeperOptions, IBeekeeperSession } from "./interfaces.js";
import { BeekeeperSession } from "./session.js";
import { safeAsyncWasmCall, safeWasmCall } from './util/wasm_error.js';

/**
 * Adds the callbacks js_callback_storage uses where a failed storage operation
 * must not fail the call (converting a legacy wallet on unlock). They report
 * failure instead of throwing: C++ cannot catch a JS exception.
 */
const withNeverThrowingCallbacks = (storage: IStorageCallbacks) => ({
  save_fn: (name: string, data: Uint8Array): void => storage.save_fn(name, data),
  load_fn: (name: string): Uint8Array => storage.load_fn(name),
  list_dir_fn: (): string[] => storage.list_dir_fn(),
  try_save_fn: (name: string, data: Uint8Array): boolean => {
    try {
      storage.save_fn(name, data);
      return true;
    } catch {
      return false;
    }
  },
  try_load_fn: (name: string): Uint8Array | null => {
    try {
      return storage.load_fn(name);
    } catch {
      return null;
    }
  }
});

// We would like to expose our api using BeekeeperInstance interface, but we would not like to expose users a way of creating instance of BeekeeperApi
export class BeekeeperApi implements IBeekeeperInstance {
  public readonly api!: Readonly<beekeeper_api>;

  public readonly sessions: Map<string, BeekeeperSession> = new Map();

  public readonly isInMemory: boolean;

  readonly #storage: IStorageCallbacks;

  /** Timeout duration in milliseconds. 0 means no timeout. */
  public readonly unlockTimeoutMs: number;

  /** Timestamp (ms) of last wallet activity. Used for TS-level timeout check. */
  #lastActivity: number = Date.now();

  /** Tail of the queue of asynchronous WASM calls, see runAsync(). */
  #queue: Promise<unknown> = Promise.resolve();

  /** True while an asynchronous WASM call is suspended inside the module. */
  #asyncCallInFlight = false;

  public constructor(
    private readonly provider: MainModule,
    private readonly options: Omit<IBeekeeperOptions, 'wasmLocation' | 'storageRoot'>,
    storage: IStorageCallbacks,
    crypto: ICryptoCallbacks
  ) {
    this.isInMemory = Boolean(options.inMemory);
    this.#storage = storage;
    this.unlockTimeoutMs = this.options.unlockTimeout * 1000;

    this.api = new this.provider.beekeeper_api(
      withNeverThrowingCallbacks(storage),
      crypto,
      this.options.unlockTimeout
    );
  }

  /**
   * Runs an asynchronous WASM call. The module suspends such calls with Asyncify,
   * which can keep only one call suspended at a time: a second call entering the
   * module while another one waits (e.g. for the KDF) corrupts its state and can
   * persist a wallet that no password opens. Async calls therefore run one at a
   * time, in the order they were made.
   */
  public runAsync<T extends () => any>(fn: T, errorCommentDuring: string): Promise<ReturnType<T>> {
    const run = this.#queue.then(async (): Promise<ReturnType<T>> => {
      this.#asyncCallInFlight = true;
      try {
        return await safeAsyncWasmCall(fn, errorCommentDuring);
      } finally {
        this.#asyncCallInFlight = false;
      }
    });
    this.#queue = run.catch(() => undefined);

    return run;
  }

  /**
   * Runs a synchronous WASM call. It cannot wait for the queue of async calls,
   * so it is refused while one of them is suspended inside the module.
   */
  public runSync<T extends () => any>(fn: T, errorCommentDuring: string): ReturnType<T> {
    if (this.#asyncCallInFlight)
      throw new BeekeeperError(`Error during ${errorCommentDuring}: another asynchronous Beekeeper call is still in progress, await it first`);

    return safeWasmCall(fn, errorCommentDuring);
  }

  /** True while an asynchronous WASM call is in progress. */
  public get asyncCallInFlight(): boolean {
    return this.#asyncCallInFlight;
  }

  /** Refresh the inactivity timer. Called on every wallet operation. */
  public refreshTimeout(): void {
    this.#lastActivity = Date.now();
  }

  /**
   * Checks if the inactivity timeout has expired and throws if so,
   * otherwise refreshes the timeout. Call this from unlocked wallet operations.
   */
  public throwIfTimedOutAndRefresh(): void {
    if (this.isTimedOut())
      throw new BeekeeperError('Wallet locked due to timeout');
    this.refreshTimeout();
  }

  /** Check if the inactivity timeout has expired. */
  public isTimedOut(): boolean {
    return this.unlockTimeoutMs > 0
      && (Date.now() - this.#lastActivity) >= this.unlockTimeoutMs;
  }

  /** Returns the timeout deadline as a Date (for getInfo). */
  public getTimeoutTime(): Date {
    if (this.unlockTimeoutMs === 0)
      return new Date('9999-12-31T23:59:59Z');

    return new Date(this.#lastActivity + this.unlockTimeoutMs);
  }

  /** Returns the list of wallet names present in persistent storage. */
  public listCreatedWallets(): string[] {
    return this.#storage.list_dir_fn();
  }

  public getVersion(): string {
    return process.env.npm_package_version as string;
  }

  public createSession(salt: string): IBeekeeperSession {
    const token = this.runSync(() => this.api.create_session(salt), "session creation");
    const session = new BeekeeperSession(this, token);

    this.sessions.set(token, session);

    return session;
  }

  public closeSession(token: string): void {
    if(!this.sessions.delete(token))
      throw new BeekeeperError(`This Beekeeper API instance is not the owner of session identified by token: "${token}"`);

    this.runSync(() => this.api.close_session(token), "session closing");
  }

  public async delete(): Promise<void> {
    // Let the calls already made finish: nothing may be suspended in the module when it is deleted.
    await this.#queue;

    for(const session of this.sessions.values())
      session.close();

    this.runSync(() => this.api.delete(), "WASM api deletion");

    await this.#storage.sync?.();
    this.#storage.close?.();
  }
}
