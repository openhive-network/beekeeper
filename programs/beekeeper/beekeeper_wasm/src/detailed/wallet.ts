import { BeekeeperApi } from "./api.js";
import { BeekeeperSession } from "./session.js";
import { IBeekeeperUnlockedWallet, IBeekeeperSession, TPublicKey, IBeekeeperWallet, TSignature } from "./interfaces.js";
import { safeAsyncWasmCall, safeWasmCall } from './util/wasm_error.js';

export class BeekeeperUnlockedWallet implements IBeekeeperUnlockedWallet {
  public constructor(
    private readonly api: BeekeeperApi,
    private readonly session: BeekeeperSession,
    private readonly locked: BeekeeperLockedWallet
  ) {}

  get name(): string {
    return this.locked.name;
  }

  get isTemporary(): boolean {
    return this.locked.isTemporary;
  }

  public lock(): BeekeeperLockedWallet {
    safeWasmCall(
      () => this.api.api.lock(this.session.token, this.locked.name),
      `wallet '${this.locked.name}' locking`
    );
    this.locked._unlocked = undefined;

    return this.locked;
  }

  public async importKey(wifKey: string): Promise<TPublicKey> {
    this.api.throwIfTimedOutAndRefresh();
    return await safeAsyncWasmCall(
      () => this.api.api.import_key(this.session.token, this.locked.name, wifKey),
      `importing key to wallet '${this.locked.name}'`
    );
  }

  public hasMatchingPrivateKey(publicKey: TPublicKey): boolean {
    this.api.throwIfTimedOutAndRefresh();
    return safeWasmCall(
      () => this.api.api.has_matching_private_key(this.session.token, this.locked.name, publicKey),
      `checking for matching key '${publicKey}' in wallet '${this.locked.name}'`
    );
  }

  public async removeKey(publicKey: TPublicKey): Promise<void> {
    this.api.throwIfTimedOutAndRefresh();
    await safeAsyncWasmCall(
      () => this.api.api.remove_key(this.session.token, this.locked.name, publicKey),
      `removing key '${publicKey}' from wallet '${this.locked.name}'`
    );
  }

  public async signDigest(publicKey: string, sigDigest: string | Uint8Array): Promise<TSignature> {
    this.api.throwIfTimedOutAndRefresh();
    if (sigDigest instanceof Uint8Array) {
      // Convert Uint8Array to hex string
      sigDigest = Array.from(sigDigest).map(b => b.toString(16).padStart(2, '0')).join('');
    }

    return await safeAsyncWasmCall(
      () => this.api.api.sign_digest(this.session.token, sigDigest, publicKey, this.locked.name),
      `signing digest with key '${publicKey}' using wallet '${this.locked.name}'`
    );
  }

  public getPublicKeys(): TPublicKey[] {
    this.api.throwIfTimedOutAndRefresh();
    const vec = safeWasmCall(
      () => this.api.api.get_public_keys(this.session.token, this.locked.name),
      `public keys retrieval from wallet '${this.locked.name}'`
    );
    return Array.from(vec);
  }

  public async encryptData(content: string, key: TPublicKey, anotherKey?: TPublicKey, nonce?: number): Promise<string> {
    this.api.throwIfTimedOutAndRefresh();
    const toKey = anotherKey ?? key;
    return await safeAsyncWasmCall(
      () => this.api.api.encrypt_data(this.session.token, this.locked.name, key, toKey, content, nonce ?? 0),
      `encrypting data in wallet '${this.locked.name}'`
    );
  }

  public async decryptData(content: string, key: TPublicKey, anotherKey?: TPublicKey): Promise<string> {
    this.api.throwIfTimedOutAndRefresh();
    const toKey = anotherKey ?? key;
    return await safeAsyncWasmCall(
      () => this.api.api.decrypt_data(this.session.token, this.locked.name, key, toKey, content),
      `decrypting data in wallet '${this.locked.name}'`
    );
  }

  public close(): IBeekeeperSession {
    return this.locked.close();
  }
}

export class BeekeeperLockedWallet implements IBeekeeperWallet {
  /** @internal Backing field for the unlocked getter. Use the `unlocked` property instead. */
  public _unlocked: BeekeeperUnlockedWallet | undefined = undefined;

  public constructor(
    private readonly api: BeekeeperApi,
    private readonly session: BeekeeperSession,
    public readonly name: string,
    public readonly isTemporary: boolean
  ) {}

  get unlocked(): BeekeeperUnlockedWallet | undefined {
    if (this._unlocked && this.api.isTimedOut()) {
      this._unlocked.lock();
    }
    return this._unlocked;
  }

  public async unlock(password: string): Promise<IBeekeeperUnlockedWallet> {
    this.api.refreshTimeout();
    await safeAsyncWasmCall(
      () => this.api.api.unlock(this.session.token, this.name, password),
      `wallet '${this.name}' unlocking`
    );
    this._unlocked = new BeekeeperUnlockedWallet(this.api, this.session, this);

    return this._unlocked;
  }

  public close(): IBeekeeperSession {
    if(typeof this._unlocked !== 'undefined')
      this._unlocked.lock();

    this.session.closeWallet(this.name);

    return this.session;
  }
}
