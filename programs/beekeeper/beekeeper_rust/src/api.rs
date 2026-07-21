//! The top-level [`BeekeeperApi`] handle.
//!
//! Equivalent of the TS `BeekeeperApi` class (`beekeeper_wasm/src/detailed/api.ts`).
//! Owns the C++ wallet manager (via [`ffi::BeekeeperHolder`]), the set of
//! open session tokens, and the activity clock used for the inactivity
//! timeout.

use std::{
    cell::RefCell,
    collections::{HashMap, HashSet},
    fs,
    path::{Path, PathBuf},
    rc::Rc,
    time::{Duration, Instant, SystemTime},
};

use cxx::UniquePtr;

use crate::{
    consts::{LEGACY_WALLET_DIR, LEGACY_WALLET_EXT},
    errors::BeekeeperError,
    ffi, new_rust_storage_protocol,
    options::BeekeeperOptions,
    session::Session,
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

/// Shared state behind a [`BeekeeperApi`] and every [`Session`] / wallet
/// handle spawned from it.
///
/// Wrapped in `Rc<RefCell<..>>` so that sessions and wallets are owned
/// values (with the session closing itself on `Drop`) rather than `&mut`
/// borrows of the api — mirroring the TS object graph, where sessions and
/// wallets keep a reference to their parent api.
pub(crate) struct Inner {
    pub(crate) holder: UniquePtr<ffi::BeekeeperHolder>,
    sessions: HashSet<String>,
    pub(crate) wallets_meta: HashMap<(String, String), WalletMeta>,
    pub(crate) is_in_memory: bool,
    unlock_timeout_ms: u32,
    last_activity: Instant,
    wallet_dir: PathBuf,
}

impl Inner {
    /// Record a wallet's metadata under `(token, name)`.
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
    pub(crate) fn wallet_is_temporary(&self, token: &str, name: &str) -> bool {
        self.wallets_meta
            .get(&(token.to_string(), name.to_string()))
            .map(|meta| meta.is_temporary)
            .unwrap_or(false)
    }

    /// Reset the inactivity clock to now.
    pub(crate) fn refresh_timeout(&mut self) {
        self.last_activity = Instant::now();
    }

    /// If the inactivity timeout has elapsed, return [`BeekeeperError::TimedOut`].
    /// Otherwise reset the clock.
    pub(crate) fn throw_if_timed_out_and_refresh(
        &mut self,
    ) -> Result<(), BeekeeperError> {
        if self.is_timed_out() {
            return Err(BeekeeperError::TimedOut);
        }
        self.refresh_timeout();

        Ok(())
    }

    /// `true` if the inactivity timeout is enabled and has elapsed.
    pub(crate) fn is_timed_out(&self) -> bool {
        self.unlock_timeout_ms > 0
            && self.last_activity.elapsed().as_millis() as u64
                >= u64::from(self.unlock_timeout_ms)
    }

    /// Wall-clock instant at which the wallet will auto-lock.
    pub(crate) fn get_timeout_time(&self) -> SystemTime {
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
    pub(crate) fn list_created_wallets(&self) -> Vec<String> {
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

    /// Close the session identified by `token` on the C++ side and discard
    /// all Rust-side bookkeeping associated with it.
    pub(crate) fn close_session(
        &mut self,
        token: &str,
    ) -> Result<(), BeekeeperError> {
        self.holder.pin_mut().close_session(token)?;
        self.forget_session(token);

        Ok(())
    }

    /// Drop the Rust-side bookkeeping for `token` without touching C++.
    pub(crate) fn forget_session(&mut self, token: &str) {
        self.sessions.remove(token);
        self.wallets_meta.retain(|(t, _), _| t != token);
    }

    /// `true` while `token` refers to a session that has not been closed yet.
    pub(crate) fn session_is_open(&self, token: &str) -> bool {
        self.sessions.contains(token)
    }
}

/// Root handle for the Beekeeper wallet daemon.
///
/// A single `BeekeeperApi` owns one C++ wallet manager and zero or more
/// sessions; each session in turn owns zero or more wallets.
///
/// Use [`BeekeeperApi::new`] to construct and
/// [`create_session`](Self::create_session) to obtain an owned
/// [`Session`](crate::session::Session) guard for further work.
///
/// # Difference from TS
///
/// TS `BeekeeperApi` keeps a `Map<string, BeekeeperSession>` keyed by
/// session token so callers can re-obtain a session object. Rust hands the
/// caller the [`Session`](crate::session::Session) value itself; the token
/// stays internal.
pub struct BeekeeperApi {
    pub(crate) inner: Rc<RefCell<Inner>>,
}

impl BeekeeperApi {
    /// Construct a new beekeeper instance.
    ///
    /// Accepts anything convertible into `Option<BeekeeperOptions>`:
    /// pass a [`BeekeeperOptions`] value directly, or `None` to use
    /// [`BeekeeperOptions::default()`] — matching the TS factory, where
    /// `beekeeperFactory()` can be called without arguments.
    ///
    /// When the options are not in-memory this also creates
    /// `<storage_root>/.beekeeper/` (if missing) for wallet files.
    ///
    /// # Difference from TS
    ///
    /// The TS constructor is `private` and reached through a factory
    /// (`createBeekeeper(...)`) that resolves a `wasmLocation`. Here the
    /// constructor is the entry point; just call it.
    pub fn new(options: impl Into<Option<BeekeeperOptions>>) -> Self {
        let options = options.into().unwrap_or_default();
        let (holder, wallet_dir) = if options.in_memory {
            (
                ffi::new_beekeeper_holder_in_memory(options.unlock_timeout),
                PathBuf::new(),
            )
        } else {
            let wallet_dir =
                Path::new(&options.storage_root).join(LEGACY_WALLET_DIR);
            let storage = new_rust_storage_protocol(&options.storage_root);
            let holder =
                ffi::new_beekeeper_holder(storage, options.unlock_timeout);
            (holder, wallet_dir)
        };

        Self {
            inner: Rc::new(RefCell::new(Inner {
                holder,
                sessions: HashSet::new(),
                wallets_meta: HashMap::new(),
                is_in_memory: options.in_memory,
                unlock_timeout_ms: options.unlock_timeout.saturating_mul(1000),
                last_activity: Instant::now(),
                wallet_dir,
            })),
        }
    }

    /// Reset the inactivity clock to now.
    ///
    /// Used internally after every successful unlocked-wallet operation.
    /// Call manually only if you have some other notion of activity to
    /// keep the wallet alive.
    pub fn refresh_timeout(&self) {
        self.inner.borrow_mut().refresh_timeout();
    }

    /// If the inactivity timeout has elapsed, return [`BeekeeperError::TimedOut`].
    /// Otherwise reset the clock.
    ///
    /// Every [`UnlockedWallet`](crate::wallet::UnlockedWallet) operation
    /// calls this as its first step, matching the TS `throwIfTimedOutAndRefresh`.
    pub fn throw_if_timed_out_and_refresh(
        &self,
    ) -> Result<(), BeekeeperError> {
        self.inner.borrow_mut().throw_if_timed_out_and_refresh()
    }

    /// `true` if the inactivity timeout is enabled and has elapsed.
    ///
    /// `unlock_timeout == 0` permanently returns `false`.
    pub fn is_timed_out(&self) -> bool {
        self.inner.borrow().is_timed_out()
    }

    /// Wall-clock instant at which the wallet will auto-lock.
    ///
    /// When the timeout is disabled this returns the UNIX epoch plus 100
    /// years (≈ year 2069) — matching the TS sentinel `9999-12-31T23:59:59Z`
    /// in spirit, just clamped to what `SystemTime` can represent on most
    /// platforms.
    pub fn get_timeout_time(&self) -> SystemTime {
        self.inner.borrow().get_timeout_time()
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
        self.inner.borrow().list_created_wallets()
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

    /// Open a new session and return it as an owned
    /// [`Session`](crate::session::Session) guard.
    ///
    /// The session closes itself (locking its wallets first) when dropped;
    /// there is no explicit `close` call.
    ///
    /// # Difference from TS
    ///
    /// - TS `createSession(salt: string)` requires a salt that's mixed into
    ///   the token. The C++ holder used here derives its own token, so the
    ///   Rust signature drops the parameter.
    /// - TS returns the session object *and* tracks it by token; Rust hands
    ///   over the only handle and keeps just the token internally so
    ///   [`delete`](Self::delete) can close stragglers.
    pub fn create_session(&self) -> Result<Session, BeekeeperError> {
        let token = {
            let mut inner = self.inner.borrow_mut();
            let token = inner.holder.pin_mut().create_session()?;
            inner.sessions.insert(token.clone());
            token
        };

        Ok(Session::new(Rc::clone(&self.inner), token))
    }

    /// Close every session, flush storage, and shut the C++ holder down.
    ///
    /// Equivalent to TS `delete()`. Despite the name there is no Rust
    /// destructor work — the [`UniquePtr<BeekeeperHolder>`](::cxx::UniquePtr)
    /// is dropped when the last handle over the shared state goes away.
    /// Calling this is useful when you want the storage flushed
    /// deterministically before the process exits.
    ///
    /// Any [`Session`](crate::session::Session) guard still alive is closed
    /// here; its later `Drop` becomes a no-op. After calling `delete` the
    /// C++ storage handle has been closed; further operations will fail.
    pub fn delete(&self) -> Result<(), BeekeeperError> {
        let mut inner = self.inner.borrow_mut();
        let tokens: Vec<String> = inner.sessions.drain().collect();
        for token in tokens {
            inner.holder.pin_mut().close_session(&token)?;
        }
        inner.wallets_meta.clear();
        inner.holder.pin_mut().sync_storage();
        inner.holder.pin_mut().close_storage();

        Ok(())
    }
}
