/**
 * Beekeeper Session - manages wallets within a session
 */

import { BeekeeperError } from "./errors.js";
import { BeekeeperApi } from "./api.js";
import { IBeekeeperInfo, IBeekeeperInstance, IBeekeeperSession, IBeekeeperWallet, IWalletCreated } from "./interfaces.js";
import { BeekeeperLockedWallet, BeekeeperUnlockedWallet } from "./wallet.js";
import { WalletState } from "./core/types.js";
import * as crypto from "./core/crypto.js";

export class BeekeeperSession implements IBeekeeperSession {
  // Map from wallet name to wallet object
  public readonly wallets: Map<string, BeekeeperLockedWallet> = new Map();
  // Map from wallet name to wallet state
  public readonly walletStates: Map<string, WalletState> = new Map();

  public constructor(
    public readonly api: BeekeeperApi,
    public readonly token: string
  ) {}

  public getInfo(): IBeekeeperInfo {
    this.api.updateActivity();
    const info = this.api.getInfo();
    return {
      now: info.now,
      timeoutTime: info.timeoutTime
    };
  }

  public listWallets(): Array<IBeekeeperWallet> {
    this.api.updateActivity();
    return Array.from(this.wallets.values());
  }

  public hasWallet(name: string): boolean {
    this.api.updateActivity();

    // Check if wallet is already opened in this session
    if (this.wallets.has(name)) {
      return true;
    }

    // Check if wallet exists in storage - this needs to be sync for interface compatibility
    // Since walletExists is async, we check the state map instead
    // User can use openWallet which will throw if wallet doesn't exist
    return this.wallets.has(name);
  }

  public async createWallet(name: string, password?: string, isTemporary?: boolean): Promise<IWalletCreated> {
    this.api.updateActivity();

    // Prevent creating persistent wallets when no filesystem is available
    if (isTemporary === false && this.api.storage === null) {
      throw new BeekeeperError(
        "Trying to create persistent wallet without a filesystem (consider disabling the 'inMemory' Beekeeper option or setting 'isTemporary' function argument to true)."
      );
    }

    // When no filesystem is available, all wallets must be temporary
    if (this.api.storage === null) {
      isTemporary = true;
    } else {
      isTemporary = isTemporary ?? false;
    }

    // Generate password if not provided
    if (password === undefined) {
      password = crypto.generatePassword(32);
    }

    // Create wallet using wallet manager
    const walletState = await this.api.walletManager.createWallet(name, password, isTemporary);
    this.walletStates.set(name, walletState);

    // Create wallet object
    const wallet = new BeekeeperLockedWallet(this, name, isTemporary);
    wallet.unlocked = new BeekeeperUnlockedWallet(this, wallet);
    this.wallets.set(name, wallet);

    return {
      wallet: wallet.unlocked,
      password
    };
  }

  public openWallet(name: string): IBeekeeperWallet {
    this.api.updateActivity();

    // Return existing wallet if already opened
    if (this.wallets.has(name)) {
      return this.wallets.get(name) as IBeekeeperWallet;
    }

    // Create wallet state (locked initially)
    const walletState: WalletState = {
      name,
      isTemporary: false,
      isUnlocked: false,
      unlockedKeys: new Map(),
      passwordHash: null
    };
    this.walletStates.set(name, walletState);

    // Create wallet object
    const wallet = new BeekeeperLockedWallet(this, name, false);
    this.wallets.set(name, wallet);

    return wallet;
  }

  public closeWallet(name: string): void {
    this.api.updateActivity();

    const wallet = this.wallets.get(name);
    if (!wallet) {
      throw new BeekeeperError(`This Beekeeper API session is not the owner of wallet identified by name: "${name}"`);
    }

    // Close wallet state
    const state = this.walletStates.get(name);
    if (state) {
      this.api.walletManager.closeWallet(state);
      this.walletStates.delete(name);
    }

    this.wallets.delete(name);
  }

  public lockAll(): Array<IBeekeeperWallet> {
    this.api.updateActivity();

    const wallets = Array.from(this.wallets.values());
    for (const wallet of wallets) {
      if (wallet.unlocked !== undefined) {
        wallet.unlocked.lock();
      }
    }
    return wallets;
  }

  public close(): IBeekeeperInstance {
    // Close all wallets
    for (const name of Array.from(this.wallets.keys())) {
      const wallet = this.wallets.get(name);
      if (wallet) {
        wallet.close();
      }
    }

    // Close session
    this.api.closeSession(this.token);

    return this.api;
  }

  /**
   * Get wallet state by name (internal use)
   */
  public getWalletState(name: string): WalletState | undefined {
    return this.walletStates.get(name);
  }
}
