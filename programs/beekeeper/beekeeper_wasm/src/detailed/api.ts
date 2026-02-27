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

  public constructor(
    private readonly provider: MainModule,
    private readonly options: Omit<IBeekeeperOptions, 'wasmLocation' | 'storageRoot'>,
    storage: IStorageCallbacks,
    crypto: ICryptoCallbacks
  ) {
    this.isInMemory = Boolean(options.inMemory);

    this.api = new this.provider.beekeeper_api(
      storage,
      crypto,
      this.options.unlockTimeout
    );
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
      await session.close();

    safeWasmCall(() => this.api.delete(), "WASM api deletion");
  }
}
