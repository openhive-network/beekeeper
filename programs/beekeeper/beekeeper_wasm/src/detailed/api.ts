import type { MainModule, beekeeper_api } from "../build/beekeeper_wasm.common";

import { BeekeeperError } from "./errors.js";
import type { ICryptoCallbacks } from "./crypto.js";
import type { IStorageCallbacks } from "./fs.js";
import { IBeekeeperInstance, IBeekeeperOptions, IBeekeeperSession } from "./interfaces.js";
import { BeekeeperSession } from "./session.js";
import { safeWasmCall } from './util/wasm_error.js';

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
      storage,
      crypto,
      this.options.unlockTimeout
    );
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

  public getVersion(): string {
    return process.env.npm_package_version as string;
  }

  public createSession(salt: string): IBeekeeperSession {
    const token = safeWasmCall(() => this.api.create_session(salt), "session creation");
    const session = new BeekeeperSession(this, token);

    this.sessions.set(token, session);

    return session;
  }

  public closeSession(token: string): void {
    if(!this.sessions.delete(token))
      throw new BeekeeperError(`This Beekeeper API instance is not the owner of session identified by token: "${token}"`);

    safeWasmCall(() => this.api.close_session(token), "session closing");
  }

  public async delete(): Promise<void> {
    for(const session of this.sessions.values())
      session.close();

    safeWasmCall(() => this.api.delete(), "WASM api deletion");

    await this.#storage.sync?.();
    this.#storage.close?.();
  }
}
