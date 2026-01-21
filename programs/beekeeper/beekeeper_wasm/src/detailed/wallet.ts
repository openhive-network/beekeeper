/**
 * Beekeeper Wallet - handles key management and signing
 */

import { BeekeeperError } from "./errors.js";
import { BeekeeperSession } from "./session.js";
import { IBeekeeperUnlockedWallet, IBeekeeperSession, TPublicKey, IBeekeeperWallet, TSignature } from "./interfaces.js";

export class BeekeeperUnlockedWallet implements IBeekeeperUnlockedWallet {
  public constructor(
    private readonly session: BeekeeperSession,
    private readonly locked: BeekeeperLockedWallet
  ) {}

  get name(): string {
    return this.locked.name;
  }

  get isTemporary(): boolean {
    return this.locked.isTemporary;
  }

  private getWalletState() {
    const state = this.session.getWalletState(this.locked.name);
    if (!state) {
      throw new BeekeeperError(`Wallet "${this.locked.name}" not found`);
    }
    return state;
  }

  public lock(): BeekeeperLockedWallet {
    this.session.api.updateActivity();

    const state = this.getWalletState();
    this.session.api.walletManager.lockWallet(state);
    this.locked.unlocked = undefined;

    return this.locked;
  }

  public async importKey(wifKey: string): Promise<TPublicKey> {
    this.session.api.updateActivity();

    const state = this.getWalletState();
    return this.session.api.walletManager.importKey(state, wifKey);
  }

  public hasMatchingPrivateKey(publicKey: TPublicKey): boolean {
    this.session.api.updateActivity();

    const state = this.getWalletState();
    return this.session.api.walletManager.hasMatchingPrivateKey(state, publicKey);
  }

  public async removeKey(publicKey: TPublicKey): Promise<void> {
    this.session.api.updateActivity();

    const state = this.getWalletState();
    await this.session.api.walletManager.removeKey(state, publicKey);
  }

  public signDigest(publicKey: string, sigDigest: string | Uint8Array): TSignature {
    this.session.api.updateActivity();

    const state = this.getWalletState();
    return this.session.api.walletManager.signDigest(state, publicKey, sigDigest);
  }

  public getPublicKeys(): TPublicKey[] {
    this.session.api.updateActivity();

    const state = this.getWalletState();
    return this.session.api.walletManager.getPublicKeys(state);
  }

  public encryptData(_content: string, _key: TPublicKey, _anotherKey?: TPublicKey, _nonce?: number): string {
    this.session.api.updateActivity();
    // SubtleCrypto is async - use encryptDataAsync instead
    throw new BeekeeperError('encryptData requires async - use encryptDataAsync instead');
  }

  public decryptData(_content: string, _key: TPublicKey, _anotherKey?: TPublicKey): string {
    this.session.api.updateActivity();
    // SubtleCrypto is async - use decryptDataAsync instead
    throw new BeekeeperError('decryptData requires async - use decryptDataAsync instead');
  }

  /**
   * Async version of encryptData
   */
  public async encryptDataAsync(content: string, key: TPublicKey, anotherKey?: TPublicKey, nonce?: number): Promise<string> {
    this.session.api.updateActivity();

    const state = this.getWalletState();
    return this.session.api.walletManager.encryptData(state, key, anotherKey || key, content, nonce);
  }

  /**
   * Async version of decryptData
   */
  public async decryptDataAsync(content: string, key: TPublicKey, anotherKey?: TPublicKey): Promise<string> {
    this.session.api.updateActivity();

    const state = this.getWalletState();
    return this.session.api.walletManager.decryptData(state, key, anotherKey || key, content);
  }

  public close(): IBeekeeperSession {
    return this.locked.close();
  }
}

export class BeekeeperLockedWallet implements IBeekeeperWallet {
  public unlocked: BeekeeperUnlockedWallet | undefined = undefined;

  public constructor(
    private readonly session: BeekeeperSession,
    public readonly name: string,
    public readonly isTemporary: boolean
  ) {}

  public unlock(_password: string): IBeekeeperUnlockedWallet {
    this.session.api.updateActivity();
    // SubtleCrypto is async - use unlockAsync instead
    throw new BeekeeperError('unlock requires async - use unlockAsync instead');
  }

  /**
   * Async version of unlock
   */
  public async unlockAsync(password: string): Promise<IBeekeeperUnlockedWallet> {
    this.session.api.updateActivity();

    const state = this.session.getWalletState(this.name);
    if (!state) {
      throw new BeekeeperError(`Wallet "${this.name}" not found`);
    }

    await this.session.api.walletManager.unlockWallet(state, password);
    this.unlocked = new BeekeeperUnlockedWallet(this.session, this);

    return this.unlocked;
  }

  public close(): IBeekeeperSession {
    if (this.unlocked !== undefined) {
      this.unlocked.lock();
    }

    this.session.closeWallet(this.name);

    return this.session;
  }
}
