import { BeekeeperError } from "./errors.js";
import { BeekeeperApi } from "./api.js";
import { IBeekeeperInfo, IBeekeeperInstance, IBeekeeperSession, IBeekeeperWallet, IWalletCreated } from "./interfaces.js";
import { BeekeeperLockedWallet, BeekeeperUnlockedWallet } from "./wallet.js";
import { safeAsyncWasmCall, safeWasmCall } from './util/wasm_error.js';

interface IBeekeeperWalletPassword {
  password: string;
}

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

  public getInfo(): IBeekeeperInfo {
    const result = this.api.extract(safeWasmCall(() => this.api.api.get_info(this.token) as string, "session info retrieval")) as IBeekeeperSessionInfo;

    return {
      now: new Date(`${result.now}Z`),
      timeoutTime: new Date(`${result.timeout_time}Z`)
    };
  }

  public hasWallet(name: string): boolean {
    if (this.wallets.has(name))
      return true;

    try {
      this.api.storageCallbacks.load_fn(name);
      return true;
    } catch {
      return false;
    }
  }

  public listWallets(): Array<IBeekeeperWallet> {
    return Array.from(this.wallets.values());
  }

  public async createWallet(name: string, password?: string, isTemporary?: boolean): Promise<IWalletCreated> {
    if (isTemporary === undefined)
      isTemporary = this.api.isInMemory;

    if(typeof password === 'string') {
      const json = await safeAsyncWasmCall(() => this.api.api.create(this.token, name, password as string) as string, `wallet '${name}' creation`);
      this.api.extract(json);
    } else {
      const json = await safeAsyncWasmCall(() => this.api.api.create(this.token, name) as string, `wallet '${name}' creation`);
      const result = this.api.extract(json) as IBeekeeperWalletPassword;
      ({ password } = result);
    }

    const wallet = new BeekeeperLockedWallet(this.api, this, name, isTemporary);
    wallet.unlocked = new BeekeeperUnlockedWallet(this.api, this, wallet);

    this.wallets.set(name, wallet);

    return {
      wallet: wallet.unlocked,
      password
    };
  }

  public openWallet(name: string): IBeekeeperWallet {
    if(this.wallets.has(name))
      return this.wallets.get(name) as IBeekeeperWallet;

    this.api.extract(safeWasmCall(() => this.api.api.open(this.token, name) as string, `wallet '${name}' opening`));
    const wallet = new BeekeeperLockedWallet(this.api, this, name, false);

    this.wallets.set(name, wallet);

    return wallet;
  }

  public closeWallet(name: string): void {
    if(!this.wallets.delete(name))
      throw new BeekeeperError(`This Beekeeper API session is not the owner of wallet identified by name: "${name}"`);

    this.api.extract(safeWasmCall(() => this.api.api.close(this.token, name) as string, `wallet '${name}' closing`));
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
