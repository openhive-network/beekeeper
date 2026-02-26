export class ExtractError extends Error {
  constructor(parsed) {
    super(`Response resulted with error: "${JSON.stringify(parsed)}"`);
    this.name = 'ExtractError';
    this.parsed = parsed;
  }
}

export class BeekeeperInstanceHelper {
  // Private properties:

  #instance = undefined;
  #implicitSessionToken = undefined;

  #acceptError = false;

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

  /**
   * @param {(arg0: boolean) => void} acceptError
   */
  set setAcceptError(acceptError) {
    this.#acceptError = acceptError;
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

  #extract(json) {
    const parsed = JSON.parse(json);

    if( this.#acceptError )
    {
      if( !parsed.hasOwnProperty('error') )
        throw new ExtractError(parsed);

      return JSON.parse(parsed.error);
    }
    else
    {
      if( !parsed.hasOwnProperty('result') )
        throw new ExtractError(parsed);

      return JSON.parse(parsed.result);
    }
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
    const returnedValue = this.instance.create_session(salt);

    const value = this.#extract(returnedValue);

    return value.token;
  }

  createSessionWithoutSalt() {
    const returnedValue = this.instance.create_session();

    const value = this.#extract(returnedValue);

    return value.token;
  }

  closeSession(token) {
    const returnedValue = this.instance.close_session(token);
    return this.#extract(returnedValue);
  }

  hasMatchingPrivateKey(token, walletName, publicKey) {
    const returnedValue = this.instance.has_matching_private_key(token, walletName, publicKey);
    const value = this.#extract(returnedValue);

    return value.exists;
  }

  hasWallet(token, walletName) {
    const returnedValue = this.instance.has_wallet(token, walletName);
    const value = this.#extract(returnedValue);

    return value.exists;
  }

  listWallets(token) {
    const returnedValue = this.instance.list_wallets(token);

    return this.#extract(returnedValue);
  }

  async create(sessionToken, walletName) {
    const returnedValue = await this.instance.create(sessionToken, walletName);

    if( this.#acceptError )
    {
      return this.#extract(returnedValue);
    }
    else
    {
      const value = this.#extract(returnedValue);
      BeekeeperInstanceHelper.#setPassword(walletName, value.password);

      return value.password;
    }
  }

  async create_with_password(sessionToken, walletName, explicitPassword) {
    const returnedValue = await this.instance.create(sessionToken, walletName, explicitPassword);

    if( this.#acceptError )
    {
      return this.#extract(returnedValue);
    }
    else
    {
      const value = this.#extract(returnedValue);
      BeekeeperInstanceHelper.#setPassword(walletName, value.password);

      return value.password;
    }
  }

  async importKey(sessionToken, walletName, key) {
    const returnedValue = await this.instance.import_key(sessionToken, walletName, key);

    if( this.#acceptError )
    {
      return this.#extract(returnedValue);
    }
    else
    {
      const value = this.#extract(returnedValue);

      return value.public_key;
    }
  };

  /**
   * @param {string} sessionToken
   * @param {string} walletName
   * @param {string} key
   */
  async removeKey(sessionToken, walletName, key) {
    const returnedValue = await this.instance.remove_key(sessionToken, walletName, key);

    return this.#extract(returnedValue);
  }

  async encryptData(sessionToken, walletName, fromPublicKey, toPublicKey, content, nonce) {
    let returnedValue;
    if (nonce !== undefined)
      returnedValue = await this.instance.encrypt_data(sessionToken, walletName, fromPublicKey, toPublicKey, content, nonce);
    else
      returnedValue = await this.instance.encrypt_data(sessionToken, walletName, fromPublicKey, toPublicKey, content);

    if( this.#acceptError )
    {
      return this.#extract(returnedValue);
    }
    else
    {
      const value = this.#extract(returnedValue);

      return value.encrypted_content;
    }
  }

  async decryptData(sessionToken, walletName, fromPublicKey, toPublicKey, encryptedContent) {
    const returnedValue = await this.instance.decrypt_data(sessionToken, walletName, fromPublicKey, toPublicKey, encryptedContent);

    if( this.#acceptError )
    {
      return this.#extract(returnedValue);
    }
    else
    {
      const value = this.#extract(returnedValue);

      return value.decrypted_content;
    }
  }

  signDigest(sessionToken, sigDigest, publicKey) {
    const returnedValue = this.instance.sign_digest(sessionToken, sigDigest, publicKey);

    if( this.#acceptError )
    {
      return this.#extract(returnedValue);
    }
    else
    {
      const value = this.#extract(returnedValue);

      return value.signature;
    }
  }

  getPublicKeys(sessionToken) {
    const returnedValue = this.instance.get_public_keys(sessionToken);

    return this.#extract(returnedValue);
  }

  getInfo(sessionToken) {
    const returnedValue = this.instance.get_info(sessionToken);

    return this.#extract(returnedValue);
  }

  open(sessionToken, walletName) {
    const returnedValue = this.instance.open(sessionToken, walletName);

    return this.#extract(returnedValue);
  }

  close(sessionToken, walletName) {
    const returnedValue = this.instance.close(sessionToken, walletName);

    return this.#extract(returnedValue);
  }

  /**
   * @param {string} sessionToken
   * @param {string} walletName
   * @param {string | null} explicitPassword
   */
  async unlock(sessionToken, walletName, explicitPassword = null) {
    const pass = ( explicitPassword == null ) ? BeekeeperInstanceHelper.#getPassword(walletName) : explicitPassword;
    const returnedValue = await this.instance.unlock(sessionToken, walletName, pass);

    return this.#extract(returnedValue);
  }

  async lock(sessionToken, walletName) {
    const returnedValue = await this.instance.lock(sessionToken, walletName);

    return this.#extract(returnedValue);
  }

  async lockAll(sessionToken) {
    const returnedValue = await this.instance.lock_all(sessionToken);

    return this.#extract(returnedValue);
  }

  deleteInstance() {
    this.instance.delete();
  }
}
