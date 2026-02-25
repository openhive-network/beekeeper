import { BeekeeperApi } from "./api.js";
import { BeekeeperSession } from "./session.js";
import { IBeekeeperUnlockedWallet, IBeekeeperSession, TPublicKey, IBeekeeperWallet, TSignature } from "./interfaces.js";
import { safeWasmCall } from './util/wasm_error.js';

interface IImportKeyResponse {
  public_key: string;
}

interface IBeekeeperSignature {
  signature: string;
}

interface IBeekeeperKeys {
  keys: Array<{
    public_key: string;
  }>;
}

interface IHasMatchingPrivateKey {
  exists: boolean;
}

interface IEncryptedContent {
  encrypted_content: string;
}

interface IDecryptedContent {
  decrypted_content: string;
}

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
    this.api.extract(safeWasmCall(() => this.api.api.lock(this.session.token, this.locked.name) as string, `wallet '${this.locked.name}' locking`));
    this.locked.unlocked = undefined;

    return this.locked;
  }

  public importKey(wifKey: string): TPublicKey {
    const { public_key } = this.api.extract(safeWasmCall(() => this.api.api.import_key(this.session.token, this.locked.name, wifKey) as string, `importing key to wallet '${this.locked.name}'`)) as IImportKeyResponse;

    return public_key;
  }

  public hasMatchingPrivateKey(publicKey: TPublicKey): boolean {
    const result = this.api.extract(safeWasmCall(() => this.api.api.has_matching_private_key(this.session.token, this.locked.name, publicKey) as string, `checking for matching key '${publicKey}' in wallet '${this.locked.name}'`)) as IHasMatchingPrivateKey;

    return result.exists;
  }

  public removeKey(publicKey: TPublicKey): void {
    this.api.extract(safeWasmCall(() => this.api.api.remove_key(this.session.token, this.locked.name, publicKey) as string, `removing key '${publicKey}' from wallet '${this.locked.name}'`));
  }

  public signDigest(publicKey: string, sigDigest: string | Uint8Array): TSignature {
    if (sigDigest instanceof Uint8Array) {
      // Convert Uint8Array to hex string
      sigDigest = Array.from(sigDigest).map(b => b.toString(16).padStart(2, '0')).join('');
    }

    const result = this.api.extract(safeWasmCall(() => this.api.api.sign_digest(this.session.token, sigDigest, publicKey) as string, `signing digest with key '${publicKey}' using wallet '${this.locked.name}'`)) as IBeekeeperSignature;

    return result.signature;
  }

  public getPublicKeys(): TPublicKey[] {
    const result = this.api.extract(safeWasmCall(() => this.api.api.get_public_keys(this.session.token, this.locked.name) as string, `public keys retrieval from wallet '${this.locked.name}'`)) as IBeekeeperKeys;

    return result.keys.map(value => value.public_key);
  }

  public encryptData(content: string, key: TPublicKey, anotherKey?: TPublicKey, nonce?: number): string {
    const toKey = anotherKey ?? key;
    const result = this.api.extract(safeWasmCall(() => {
      if (nonce !== undefined)
        return this.api.api.encrypt_data(this.session.token, this.locked.name, key, toKey, content, nonce) as string;
      return this.api.api.encrypt_data(this.session.token, this.locked.name, key, toKey, content) as string;
    }, `encrypting data in wallet '${this.locked.name}'`)) as IEncryptedContent;

    return result.encrypted_content;
  }

  public decryptData(content: string, key: TPublicKey, anotherKey?: TPublicKey): string {
    const toKey = anotherKey ?? key;
    const result = this.api.extract(safeWasmCall(() =>
      this.api.api.decrypt_data(this.session.token, this.locked.name, key, toKey, content) as string,
    `decrypting data in wallet '${this.locked.name}'`)) as IDecryptedContent;

    return result.decrypted_content;
  }

  public close(): IBeekeeperSession {
    return this.locked.close();
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

  public unlock(password: string): IBeekeeperUnlockedWallet {
    this.api.extract(safeWasmCall(() => this.api.api.unlock(this.session.token, this.name, password) as string, `wallet '${this.name}' unlocking`));
    this.unlocked = new BeekeeperUnlockedWallet(this.api, this.session, this);

    return this.unlocked;
  }

  public close(): IBeekeeperSession {
    if(typeof this.unlocked !== 'undefined')
      this.unlocked.lock();

    this.session.closeWallet(this.name);

    return this.session;
  }
}
