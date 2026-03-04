import { BeekeeperApi } from "./api.js";
import { BeekeeperSession } from "./session.js";
import { IBeekeeperUnlockedWallet, IBeekeeperSession, TPublicKey, IBeekeeperWallet, TSignature } from "./interfaces.js";
import { safeAsyncWasmCall } from './util/wasm_error.js';

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

  public async lock(): Promise<BeekeeperLockedWallet> {
    await safeAsyncWasmCall(
      () => this.api.api.lock(this.session.token, this.locked.name),
      `wallet '${this.locked.name}' locking`
    );
    this.locked.unlocked = undefined;

    return this.locked;
  }

  public async importKey(wifKey: string): Promise<TPublicKey> {
    return await safeAsyncWasmCall(
      () => this.api.api.import_key(this.session.token, this.locked.name, wifKey),
      `importing key to wallet '${this.locked.name}'`
    );
  }

  public async hasMatchingPrivateKey(publicKey: TPublicKey): Promise<boolean> {
    return await safeAsyncWasmCall(
      () => this.api.api.has_matching_private_key(this.session.token, this.locked.name, publicKey),
      `checking for matching key '${publicKey}' in wallet '${this.locked.name}'`
    );
  }

  public async removeKey(publicKey: TPublicKey): Promise<void> {
    await safeAsyncWasmCall(
      () => this.api.api.remove_key(this.session.token, this.locked.name, publicKey),
      `removing key '${publicKey}' from wallet '${this.locked.name}'`
    );
  }

  public async signDigest(publicKey: string, sigDigest: string | Uint8Array): Promise<TSignature> {
    if (sigDigest instanceof Uint8Array) {
      // Convert Uint8Array to hex string
      sigDigest = Array.from(sigDigest).map(b => b.toString(16).padStart(2, '0')).join('');
    }

    return await safeAsyncWasmCall(
      () => this.api.api.sign_digest(this.session.token, sigDigest, publicKey, this.locked.name),
      `signing digest with key '${publicKey}' using wallet '${this.locked.name}'`
    );
  }

  public async getPublicKeys(): Promise<TPublicKey[]> {
    const vec = await safeAsyncWasmCall(
      () => this.api.api.get_public_keys(this.session.token, this.locked.name),
      `public keys retrieval from wallet '${this.locked.name}'`
    );
    return Array.from(vec);
  }

  public async encryptData(content: string, key: TPublicKey, anotherKey?: TPublicKey, nonce?: number): Promise<string> {
    const toKey = anotherKey ?? key;
    return await safeAsyncWasmCall(
      () => this.api.api.encrypt_data(this.session.token, this.locked.name, key, toKey, content, nonce ?? 0),
      `encrypting data in wallet '${this.locked.name}'`
    );
  }

  public async decryptData(content: string, key: TPublicKey, anotherKey?: TPublicKey): Promise<string> {
    const toKey = anotherKey ?? key;
    return await safeAsyncWasmCall(
      () => this.api.api.decrypt_data(this.session.token, this.locked.name, key, toKey, content),
      `decrypting data in wallet '${this.locked.name}'`
    );
  }

  public async close(): Promise<IBeekeeperSession> {
    return await this.locked.close();
  }
}

export class BeekeeperLockedWallet implements IBeekeeperWallet {
  public unlocked: BeekeeperUnlockedWallet | undefined = undefined;

  public constructor(
    private readonly api: BeekeeperApi,
    private readonly session: BeekeeperSession,
    public readonly name: string,
    public readonly isTemporary: boolean
  ) {}

  public async unlock(password: string): Promise<IBeekeeperUnlockedWallet> {
    await safeAsyncWasmCall(
      () => this.api.api.unlock(this.session.token, this.name, password),
      `wallet '${this.name}' unlocking`
    );
    this.unlocked = new BeekeeperUnlockedWallet(this.api, this.session, this);

    return this.unlocked;
  }

  public async close(): Promise<IBeekeeperSession> {
    if(typeof this.unlocked !== 'undefined')
      await this.unlocked.lock();

    await this.session.closeWallet(this.name);

    return this.session;
  }
}
