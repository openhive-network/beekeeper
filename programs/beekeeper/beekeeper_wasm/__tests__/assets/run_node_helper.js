export class BeekeeperInstanceHelper {
  // Private properties:

  #instance = undefined;
  #implicitSessionToken = undefined;

  static #passwords = new Map();

  /** Crypto callbacks for the WASM beekeeper_api constructor (4th arg). */
  static cryptoCallbacks = undefined;

  // Getters for private properties:

  get implicitSessionToken() {
    return this.#implicitSessionToken;
  }

  get instance() {
    return this.#instance;
  }

  // Private functions:

  static #setPassword(walletName, password) {
    BeekeeperInstanceHelper.#passwords.set(walletName, password);
  }

  static #getPassword(walletName) {
    const pass = BeekeeperInstanceHelper.#passwords.get(walletName);
    if(!pass)
      throw new Error("Wallet does not exist");

    return pass;
  }

  /**
   * Parse options array (--wallet-dir X --unlock-timeout Y --enable-logs Z)
   * into { walletDir, unlockTimeout } for the new minimal constructor.
   */
  static #parseOptions(options) {
    let walletDir = '.';
    let unlockTimeout = 900;

    for (let i = 0; i < options.length; i++) {
      if (options[i] === '--wallet-dir' && i + 1 < options.length) {
        walletDir = options[++i];
      } else if (options[i] === '--unlock-timeout' && i + 1 < options.length) {
        unlockTimeout = parseInt(options[++i], 10);
      }
      // --enable-logs is ignored (no logging in core_minimal)
    }

    return { walletDir, unlockTimeout };
  }

  /**
   * Prepares beekeeper instance helper for given provider
   * @returns {BeekeeperInstanceHelper}
   */
  static for(provider) {
    return BeekeeperInstanceHelper.bind(undefined, provider);
  }

  /**
   * Creates in-memory save/load/list_dir callbacks backed by a Map.
   * @returns {{ save_fn: Function, load_fn: Function, list_dir_fn: Function }}
   */
  static createInMemoryStorage() {
    const store = new Map();
    const save_fn = (name, data) => { store.set(name, new Uint8Array(data)); };
    const load_fn = (name) => {
      const d = store.get(name);
      if (!d) throw new Error("Wallet not found: " + name);
      return d;
    };
    const list_dir_fn = () => Array.from(store.keys());
    return { save_fn, load_fn, list_dir_fn, store };
  }

  constructor(provider, options, storageFns) {
    const { unlockTimeout } = BeekeeperInstanceHelper.#parseOptions(options);
    const storage = storageFns || BeekeeperInstanceHelper.createInMemoryStorage();
    const crypto = BeekeeperInstanceHelper.cryptoCallbacks;
    if (crypto)
      this.#instance = new provider.beekeeper_api(storage, crypto, unlockTimeout);
    else
      this.#instance = new provider.beekeeper_api(storage, unlockTimeout);
    this.#implicitSessionToken = this.createSessionWithoutSalt();
  }

  // Public helper methods:

  createSession(salt) {
    return this.instance.create_session(salt);
  }

  createSessionWithoutSalt() {
    return this.instance.create_session("");
  }

  async closeSession(token) {
    await this.instance.close_session(token);
  }

  async hasMatchingPrivateKey(token, walletName, publicKey) {
    return await this.instance.has_matching_private_key(token, walletName, publicKey);
  }

  async hasWallet(token, walletName) {
    return await this.instance.has_wallet(token, walletName);
  }

  async listWallets(token) {
    const wallets = Array.from(await this.instance.list_wallets(token));
    return { wallets };
  }

  async create(sessionToken, walletName) {
    const password = await this.instance.create(sessionToken, walletName, "", false);
    BeekeeperInstanceHelper.#setPassword(walletName, password);

    return password;
  }

  async create_with_password(sessionToken, walletName, explicitPassword) {
    const password = await this.instance.create(sessionToken, walletName, explicitPassword, false);
    BeekeeperInstanceHelper.#setPassword(walletName, password);

    return password;
  }

  async importKey(sessionToken, walletName, key) {
    return await this.instance.import_key(sessionToken, walletName, key);
  };

  /**
   * @param {string} sessionToken
   * @param {string} walletName
   * @param {string} key
   */
  async removeKey(sessionToken, walletName, key) {
    await this.instance.remove_key(sessionToken, walletName, key);
  }

  async encryptData(sessionToken, walletName, fromPublicKey, toPublicKey, content, nonce) {
    return await this.instance.encrypt_data(sessionToken, walletName, fromPublicKey, toPublicKey, content, nonce ?? 0);
  }

  async decryptData(sessionToken, walletName, fromPublicKey, toPublicKey, encryptedContent) {
    return await this.instance.decrypt_data(sessionToken, walletName, fromPublicKey, toPublicKey, encryptedContent);
  }

  async signDigest(sessionToken, sigDigest, publicKey) {
    return await this.instance.sign_digest(sessionToken, sigDigest, publicKey, "");
  }

  async getPublicKeys(sessionToken) {
    const keys = Array.from(await this.instance.get_public_keys(sessionToken, ""));
    // Wrap flat string[] into {keys: [{public_key: ...}]} for test compatibility
    return { keys: keys.map(k => ({ public_key: k })) };
  }

  async getInfo(sessionToken) {
    return await this.instance.get_info(sessionToken);
  }

  async open(sessionToken, walletName) {
    await this.instance.open(sessionToken, walletName);
  }

  async close(sessionToken, walletName) {
    await this.instance.close(sessionToken, walletName);
  }

  /**
   * @param {string} sessionToken
   * @param {string} walletName
   * @param {string | null} explicitPassword
   */
  async unlock(sessionToken, walletName, explicitPassword = null) {
    const pass = ( explicitPassword == null ) ? BeekeeperInstanceHelper.#getPassword(walletName) : explicitPassword;
    await this.instance.unlock(sessionToken, walletName, pass);
  }

  async lock(sessionToken, walletName) {
    await this.instance.lock(sessionToken, walletName);
  }

  async lockAll(sessionToken) {
    await this.instance.lock_all(sessionToken);
  }

  deleteInstance() {
    this.instance.delete();
  }
}
