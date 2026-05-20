//! The top-level [`BeekeeperApi`] handle.
//!
//! Equivalent of the TS `BeekeeperApi` class (`beekeeper_wasm/src/detailed/api.ts`).
//! Owns the C++ wallet manager (via [`ffi::BeekeeperHolder`]), the set of
//! open session tokens, and the activity clock used for the inactivity
//! timeout.

use std::{
    collections::{HashMap, HashSet},
    fs,
    path::{Path, PathBuf},
    time::{Duration, Instant, SystemTime},
};

use cxx::UniquePtr;

use crate::{
    RustCryptoProtocol,
    consts::{LEGACY_WALLET_DIR, LEGACY_WALLET_EXT},
    errors::BeekeeperError,
    ffi, new_rust_storage_protocol,
    options::BeekeeperOptions,
};

/// Per-wallet metadata that the C++ layer does not track.
///
/// Currently just the `is_temporary` flag (an in-memory-only wallet inside
/// a persistent beekeeper). Stored on the Rust side because the C++ holder
/// returns only `(name, unlocked)` pairs from `list_wallets`.
#[derive(Clone, Copy)]
pub(crate) struct WalletMeta {
    pub is_temporary: bool,
}

/// Root handle for the Beekeeper wallet daemon.
///
/// A single `BeekeeperApi` owns one C++ wallet manager and zero or more
/// sessions; each session in turn owns zero or more wallets.
///
/// Use [`BeekeeperApi::new`] to construct, [`create_session`](Self::create_session)
/// to start a session, then [`session`](Self::session) to obtain a borrowed
/// [`Session`](crate::session::Session) for further work.
///
/// # Difference from TS
///
/// - TS `BeekeeperApi` is `async`-friendly and exposes
///   `Map<string, BeekeeperSession>` so callers can re-obtain a session
///   object by token. Rust stores only the token strings; the
///   [`Session`](crate::session::Session) wrapper is rebuilt on demand
///   from [`session`](Self::session).
/// - TS calls `await this.#storage.sync?.()` in `delete()`. Rust calls
///   `sync_storage` + `close_storage` on the C++ holder synchronously.
pub struct BeekeeperApi {
    pub(super) holder: UniquePtr<ffi::BeekeeperHolder>,
    sessions: HashSet<String>,
    pub(crate) wallets_meta: HashMap<(String, String), WalletMeta>,
    pub(super) is_in_memory: bool,
    unlock_timeout_ms: u32,
    last_activity: Instant,
    wallet_dir: PathBuf,
}

impl BeekeeperApi {
    /// Construct a new beekeeper instance from the given options.
    ///
    /// When `options.in_memory` is `false` this also creates
    /// `<storage_root>/.beekeeper/` (if missing) for wallet files.
    ///
    /// # Difference from TS
    ///
    /// The TS constructor is `private` and reached through a factory
    /// (`createBeekeeper(...)`) that resolves a `wasmLocation`. Here the
    /// constructor is the entry point; just call it.
    pub fn new(options: BeekeeperOptions) -> Self {
        let crypto = Box::new(RustCryptoProtocol);
        let (holder, wallet_dir) = if options.in_memory {
            (
                ffi::new_beekeeper_holder_in_memory(
                    crypto,
                    options.unlock_timeout,
                ),
                PathBuf::new(),
            )
        } else {
            let wallet_dir =
                Path::new(&options.storage_root).join(LEGACY_WALLET_DIR);
            let storage = new_rust_storage_protocol(&options.storage_root);
            let holder = ffi::new_beekeeper_holder(
                crypto,
                storage,
                options.unlock_timeout,
            );
            (holder, wallet_dir)
        };

        Self {
            holder,
            sessions: HashSet::new(),
            wallets_meta: HashMap::new(),
            is_in_memory: options.in_memory,
            unlock_timeout_ms: options.unlock_timeout.saturating_mul(1000),
            last_activity: Instant::now(),
            wallet_dir,
        }
    }

    /// Record a wallet's metadata under `(token, name)`.
    ///
    /// Called from [`Session::create_wallet`](crate::session::Session::create_wallet)
    /// and [`Session::open_wallet`](crate::session::Session::open_wallet)
    /// so [`Session::list_wallets`](crate::session::Session::list_wallets)
    /// can attach `is_temporary` later.
    pub(crate) fn register_wallet(
        &mut self,
        token: &str,
        name: &str,
        is_temporary: bool,
    ) {
        self.wallets_meta.insert(
            (token.to_string(), name.to_string()),
            WalletMeta { is_temporary },
        );
    }

    /// Drop the metadata entry recorded by [`register_wallet`](Self::register_wallet).
    pub(crate) fn unregister_wallet(&mut self, token: &str, name: &str) {
        self.wallets_meta
            .remove(&(token.to_string(), name.to_string()));
    }

    /// Look up whether `(token, name)` was registered as a temporary wallet.
    ///
    /// Returns `false` when the wallet was never registered (e.g. a wallet
    /// reported by `list_wallets` on the C++ side but never opened through
    /// the Rust API).
    pub(crate) fn wallet_is_temporary(&self, token: &str, name: &str) -> bool {
        self.wallets_meta
            .get(&(token.to_string(), name.to_string()))
            .map(|meta| meta.is_temporary)
            .unwrap_or(false)
    }

    /// Reset the inactivity clock to now.
    ///
    /// Used internally after every successful unlocked-wallet operation.
    /// Call manually only if you have some other notion of activity to
    /// keep the wallet alive.
    pub fn refresh_timeout(&mut self) {
        self.last_activity = Instant::now();
    }

    /// If the inactivity timeout has elapsed, return [`BeekeeperError::TimedOut`].
    /// Otherwise reset the clock.
    ///
    /// Every [`UnlockedWallet`](crate::wallet::UnlockedWallet) operation
    /// calls this as its first step, matching the TS `throwIfTimedOutAndRefresh`.
    pub fn throw_if_timed_out_and_refresh(
        &mut self,
    ) -> Result<(), BeekeeperError> {
        if self.is_timed_out() {
            return Err(BeekeeperError::TimedOut);
        }
        self.refresh_timeout();

        Ok(())
    }

    /// `true` if the inactivity timeout is enabled and has elapsed.
    ///
    /// `unlock_timeout == 0` permanently returns `false`.
    pub fn is_timed_out(&self) -> bool {
        self.unlock_timeout_ms > 0
            && self.last_activity.elapsed().as_millis() as u64
                >= u64::from(self.unlock_timeout_ms)
    }

    /// Wall-clock instant at which the wallet will auto-lock.
    ///
    /// When the timeout is disabled this returns the UNIX epoch plus 100
    /// years (≈ year 2069) — matching the TS sentinel `9999-12-31T23:59:59Z`
    /// in spirit, just clamped to what `SystemTime` can represent on most
    /// platforms.
    pub fn get_timeout_time(&self) -> SystemTime {
        if self.unlock_timeout_ms == 0 {
            return SystemTime::UNIX_EPOCH
                + Duration::from_secs(60 * 60 * 24 * 365 * 100);
        }
        let elapsed = self.last_activity.elapsed();
        let remaining =
            Duration::from_millis(u64::from(self.unlock_timeout_ms))
                .saturating_sub(elapsed);

        SystemTime::now() + remaining
    }

    /// Names of every wallet found in the storage directory.
    ///
    /// Returns `[]` in in-memory mode, since there is nothing to scan. The
    /// names are derived from filenames by stripping the `.wallet` suffix;
    /// other files in the directory are ignored.
    ///
    /// # Difference from TS
    ///
    /// TS delegates to `storage.list_dir_fn()` so a custom storage backend
    /// can decide what counts as "a wallet". Rust hard-codes a filesystem
    /// scan against the `.beekeeper/` sub-directory.
    pub fn list_created_wallets(&self) -> Vec<String> {
        if self.is_in_memory {
            return Vec::new();
        }
        let Ok(entries) = fs::read_dir(&self.wallet_dir) else {
            return Vec::new();
        };
        entries
            .filter_map(|e| e.ok())
            .filter_map(|e| {
                e.file_name()
                    .to_str()?
                    .strip_suffix(LEGACY_WALLET_EXT)
                    .map(String::from)
            })
            .collect()
    }

    /// The crate's SemVer string (compiled-in `CARGO_PKG_VERSION`).
    ///
    /// # Difference from TS
    ///
    /// TS reads `process.env.npm_package_version` at runtime; the Rust
    /// value is baked in at compile time.
    pub fn version(&self) -> &'static str {
        env!("CARGO_PKG_VERSION")
    }

    /// Open a new session and return its token.
    ///
    /// The returned token is owned by the caller — pass it to
    /// [`session`](Self::session) or [`close_session`](Self::close_session)
    /// to interact with it.
    ///
    /// # Difference from TS
    ///
    /// TS `createSession(salt: string)` requires a salt that's mixed into
    /// the token. The C++ holder used here derives its own token, so the
    /// Rust signature drops the parameter.
    pub fn create_session(&mut self) -> Result<String, BeekeeperError> {
        let token = self.holder.pin_mut().create_session()?;
        self.sessions.insert(token.clone());

        Ok(token)
    }

    /// Borrow this beekeeper as a [`Session`](crate::session::Session) for
    /// the duration of one operation chain.
    ///
    /// The returned `Session` mutably borrows `self`; you cannot hold two
    /// `Session`s at once. This is intentional — it forces operations to
    /// be serialised, mirroring the C++ wallet manager's single-threaded
    /// assumption.
    ///
    /// `token` is **not** validated here; passing an unknown token simply
    /// causes later calls to fail with [`BeekeeperError::Cxx`].
    pub fn session<'a>(
        &'a mut self,
        token: &'a str,
    ) -> crate::session::Session<'a> {
        crate::session::Session { bk: self, token }
    }

    /// Close the session identified by `token`.
    ///
    /// Discards all wallet metadata associated with the token. The C++
    /// holder closes any wallets that were open in that session.
    pub fn close_session(&mut self, token: &str) -> Result<(), BeekeeperError> {
        self.holder.pin_mut().close_session(token)?;
        self.sessions.remove(token);
        self.wallets_meta.retain(|(t, _), _| t != token);

        Ok(())
    }

    /// Close every session, flush storage, and shut the C++ holder down.
    ///
    /// Equivalent to TS `delete()`. Despite the name there is no Rust
    /// destructor work — the [`UniquePtr<BeekeeperHolder>`](::cxx::UniquePtr)
    /// is dropped when `BeekeeperApi` itself is dropped. Calling this is
    /// useful when you want the storage flushed deterministically before
    /// the process exits.
    ///
    /// After calling `delete` the instance is left in a state where the C++
    /// storage handle has been closed; further operations will fail.
    pub fn delete(&mut self) -> Result<(), BeekeeperError> {
        let tokens: Vec<String> = self.sessions.drain().collect();
        for token in tokens {
            self.holder.pin_mut().close_session(&token)?;
        }
        self.wallets_meta.clear();
        self.holder.pin_mut().sync_storage();
        self.holder.pin_mut().close_storage();

        Ok(())
    }
}
