import { BeekeeperError } from "./errors.js";
import { BeekeeperApi } from "./api.js";
import { IBeekeeperInfo, IBeekeeperInstance, IBeekeeperSession, IBeekeeperWallet, IWalletCreated } from "./interfaces.js";
import { BeekeeperLockedWallet, BeekeeperUnlockedWallet } from "./wallet.js";
import { safeAsyncWasmCall } from './util/wasm_error.js';

interface IBeekeeperSessionInfo {
  now: string;
  timeout_time: string;
}

export class BeekeeperSession implements IBeekeeperSession {
  public constructor(
    private readonly api: BeekeeperApi,
    public readonly token: string
  ) {}

  public readonly wallets: Map<string, BeekeeperLockedWallet> = new Map();

  public async getInfo(): Promise<IBeekeeperInfo> {
    const result = await safeAsyncWasmCall(
      () => this.api.api.get_info(this.token),
      "session info retrieval"
    ) as IBeekeeperSessionInfo;

    return {
      now: new Date(`${result.now}Z`),
      timeoutTime: new Date(`${result.timeout_time}Z`)
    };
  }

  public async hasWallet(name: string): Promise<boolean> {
    return await safeAsyncWasmCall(
      () => this.api.api.has_wallet(this.token, name),
      `wallet '${name}' existence check`
    );
  }

  public listWallets(): Array<IBeekeeperWallet> {
    return Array.from(this.wallets.values());
  }

  public async createWallet(name: string, password?: string, isTemporary?: boolean): Promise<IWalletCreated> {
    if (isTemporary === undefined)
      isTemporary = this.api.isInMemory;

    const returnedPassword: string = await safeAsyncWasmCall(
      () => this.api.api.create(this.token, name, password ?? "", isTemporary),
      `wallet '${name}' creation`
    );
    if (typeof password !== 'string')
      password = returnedPassword;

    const wallet = new BeekeeperLockedWallet(this.api, this, name, isTemporary);
    wallet.unlocked = new BeekeeperUnlockedWallet(this.api, this, wallet);

    this.wallets.set(name, wallet);

    return {
      wallet: wallet.unlocked,
      password
    };
  }

  public async openWallet(name: string): Promise<IBeekeeperWallet> {
    if(this.wallets.has(name))
      return this.wallets.get(name) as IBeekeeperWallet;

    await safeAsyncWasmCall(
      () => this.api.api.open(this.token, name),
      `wallet '${name}' opening`
    );
    const wallet = new BeekeeperLockedWallet(this.api, this, name, false);

    this.wallets.set(name, wallet);

    return wallet;
  }

  public async closeWallet(name: string): Promise<void> {
    if(!this.wallets.delete(name))
      throw new BeekeeperError(`This Beekeeper API session is not the owner of wallet identified by name: "${name}"`);

    await safeAsyncWasmCall(
      () => this.api.api.close(this.token, name),
      `wallet '${name}' closing`
    );
  }

  public async lockAll(): Promise<Array<IBeekeeperWallet>> {
    const wallets = Array.from(this.wallets.values());
    for(const wallet of wallets)
      if(typeof wallet.unlocked !== 'undefined')
        await wallet.unlocked.lock();

    return wallets;
  }

  public async close(): Promise<IBeekeeperInstance> {
    for(const wallet of this.wallets.values())
      await wallet.close();

    this.api.closeSession(this.token);

    return this.api;
  }
}
