import { BeekeeperError } from "./errors.js";
import { BeekeeperApi } from "./api.js";
import { IBeekeeperInfo, IBeekeeperInstance, IBeekeeperSession, IBeekeeperWallet, IWalletCreated } from "./interfaces.js";
import { BeekeeperLockedWallet, BeekeeperUnlockedWallet } from "./wallet.js";

export class BeekeeperSession implements IBeekeeperSession {
  public constructor(
    private readonly api: BeekeeperApi,
    public readonly token: string
  ) {}

  public readonly wallets: Map<string, BeekeeperLockedWallet> = new Map();

  public getInfo(): IBeekeeperInfo {
    return {
      now: new Date(),
      timeoutTime: this.api.getTimeoutTime()
    };
  }

  public hasWallet(name: string): boolean {
    return this.api.runSync(
      () => this.api.api.has_wallet(this.token, name),
      `wallet '${name}' existence check`
    );
  }

  public listWallets(): Array<IBeekeeperWallet> {
    return Array.from(this.wallets.values());
  }

  public listCreatedWallets(): Array<IBeekeeperWallet> {
    return this.api.listCreatedWallets().map(name =>
      this.wallets.get(name) ?? new BeekeeperLockedWallet(this.api, this, name, false, false)
    );
  }

  public async createWallet(name: string, password?: string, isTemporary?: boolean): Promise<IWalletCreated> {
    if (isTemporary === undefined)
      isTemporary = this.api.isInMemory;

    const returnedPassword: string = await this.api.runAsync(
      () => this.api.api.create(this.token, name, password ?? "", isTemporary),
      `wallet '${name}' creation`
    );
    if (typeof password !== 'string')
      password = returnedPassword;

    const wallet = new BeekeeperLockedWallet(this.api, this, name, isTemporary);
    wallet._unlocked = new BeekeeperUnlockedWallet(this.api, this, wallet);

    this.wallets.set(name, wallet);

    return {
      wallet: wallet._unlocked!,
      password
    };
  }

  public openWallet(name: string): IBeekeeperWallet {
    if(this.wallets.has(name))
      return this.wallets.get(name) as IBeekeeperWallet;

    // While another call is in progress the module cannot be entered; the wallet is
    // then opened by its first unlock(), which runs after that call.
    const opened = !this.api.asyncCallInFlight;
    if (opened)
      this.api.runSync(
        () => this.api.api.open(this.token, name),
        `wallet '${name}' opening`
      );
    const wallet = new BeekeeperLockedWallet(this.api, this, name, false, opened);

    this.wallets.set(name, wallet);

    return wallet;
  }

  public closeWallet(name: string): void {
    if(!this.wallets.delete(name))
      throw new BeekeeperError(`This Beekeeper API session is not the owner of wallet identified by name: "${name}"`);

    if (this.api.asyncCallInFlight) {
      // Close right after the calls in progress (see BeekeeperUnlockedWallet.lock()).
      this.api.runAsync(
        () => this.api.api.close(this.token, name),
        `wallet '${name}' closing`
      ).catch(() => undefined);
      return;
    }

    this.api.runSync(
      () => this.api.api.close(this.token, name),
      `wallet '${name}' closing`
    );
  }

  public lockAll(): Array<IBeekeeperWallet> {
    const wallets = Array.from(this.wallets.values());
    for(const wallet of wallets)
      if(typeof wallet.unlocked !== 'undefined')
        wallet.unlocked.lock();

    return wallets;
  }

  public close(): IBeekeeperInstance {
    for(const wallet of this.wallets.values())
      wallet.close();

    this.api.closeSession(this.token);

    return this.api;
  }
}
