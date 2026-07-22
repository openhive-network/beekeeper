//! The top-level [`BeekeeperApi`] handle.
//!
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

/// Auto-lock instant reported while the inactivity timeout is disabled:
/// a far-future sentinel of UNIX epoch + 100 years.
const NO_TIMEOUT_SENTINEL: Duration =
    Duration::from_secs(60 * 60 * 24 * 365 * 100);

/// Root handle for the Beekeeper wallet daemon.
///
/// A single `BeekeeperApi` owns one C++ wallet manager and zero or more
/// sessions; each session in turn owns zero or more wallets.
///
/// Use [`BeekeeperApi::new`] to construct and
/// [`create_session`](Self::create_session) to obtain an owned
/// [`Session`](crate::session::Session) guard for further work.
pub struct BeekeeperApi {
    pub(crate) inner: Rc<RefCell<Inner>>,
}

/// Shared state behind a [`BeekeeperApi`] and every [`Session`] / wallet
/// handle spawned from it.
///
/// Wrapped in `Rc<RefCell<..>>` so that sessions and wallets are owned
/// values (with the session closing itself on `Drop`) rather than `&mut`
/// borrows of the api.
pub(crate) struct Inner {
    pub(crate) holder: UniquePtr<ffi::BeekeeperHolder>,
    sessions: HashSet<String>,
    wallets_meta: HashMap<(String, String), WalletMeta>,
    pub(crate) is_in_memory: bool,
    unlock_timeout_ms: u32,
    last_activity: Instant,
    wallet_dir: PathBuf,
}

/// Per-wallet metadata that the C++ layer does not track.
///
/// Currently just the `is_temporary` flag (an in-memory-only wallet inside
/// a persistent beekeeper). Stored on the Rust side because the C++ holder
/// returns only `(name, unlocked)` pairs from `list_wallets`.
#[derive(Clone, Copy)]
pub(crate) struct WalletMeta {
    pub is_temporary: bool,
}

impl BeekeeperApi {
    /// Construct a new beekeeper instance.
    ///
    /// Accepts anything convertible into `Option<BeekeeperOptions>`:
    /// pass a [`BeekeeperOptions`] value directly, or `None` to use
    /// [`BeekeeperOptions::default()`].
    ///
    /// When the options are not in-memory this also creates
    /// `<storage_root>/.beekeeper/` (if missing) for wallet files.
    pub fn new(options: impl Into<Option<BeekeeperOptions>>) -> Self {
        let options = options.into().unwrap_or_default();
        let inner = Inner::new(&options);

        Self {
            inner: Rc::new(RefCell::new(inner)),
        }
    }

    /// Open a new session and return it as an owned
    /// [`Session`](crate::session::Session) guard.
    ///
    /// The session closes itself (locking its wallets first) when dropped;
    /// there is no explicit `close` call.
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
    /// Named after the TS `delete()` API; there is no Rust destructor work
    /// behind it. Calling it is useful when you want the storage flushed
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
    /// calls this as its first step.
    pub fn throw_if_timed_out_and_refresh(&self) -> Result<(), BeekeeperError> {
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
    /// When the timeout is disabled this returns a far-future sentinel:
    /// the UNIX epoch plus 100 years (≈ year 2069).
    pub fn get_timeout_time(&self) -> SystemTime {
        self.inner.borrow().get_timeout_time()
    }

    /// Names of every wallet found in the storage directory.
    ///
    /// Returns `[]` in in-memory mode, since there is nothing to scan. The
    /// names are derived from filenames by stripping the `.wallet` suffix;
    /// other files in the directory are ignored.
    pub fn list_created_wallets(&self) -> Vec<String> {
        self.inner.borrow().list_created_wallets()
    }

    /// The crate's SemVer string (compiled-in `CARGO_PKG_VERSION`).
    pub fn version(&self) -> &'static str {
        env!("CARGO_PKG_VERSION")
    }
}

impl Inner {
    fn new(options: &BeekeeperOptions) -> Self {
        let (holder, wallet_dir) = new_holder(options);

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
            return SystemTime::UNIX_EPOCH + NO_TIMEOUT_SENTINEL;
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

    /// Lock every unlocked wallet in the session and return the (pre-lock)
    /// holder-side listing.
    pub(crate) fn lock_session_wallets(
        &mut self,
        token: &str,
    ) -> Result<Vec<ffi::WalletDetails>, BeekeeperError> {
        let wallets = self.holder.list_wallets(token)?;
        for wd in wallets.iter().filter(|w| w.unlocked) {
            self.holder.pin_mut().lock(&wd.name)?;
        }

        Ok(wallets)
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

    /// Best-effort session teardown used by `Session::drop`: lock the
    /// session's wallets, then close the session, ignoring errors.
    ///
    /// No-op when the session was already closed (e.g. by
    /// [`BeekeeperApi::delete`]).
    pub(crate) fn close_session_quietly(&mut self, token: &str) {
        if !self.session_is_open(token) {
            return;
        }
        let _ = self.lock_session_wallets(token);
        let _ = self.close_session(token);
    }

    /// Drop the Rust-side bookkeeping for `token` without touching C++.
    fn forget_session(&mut self, token: &str) {
        self.sessions.remove(token);
        self.wallets_meta.retain(|(t, _), _| t != token);
    }

    /// `true` while `token` refers to a session that has not been closed yet.
    fn session_is_open(&self, token: &str) -> bool {
        self.sessions.contains(token)
    }
}

/// Build the C++ holder matching `options`, together with the wallet
/// directory the persistent backend stores its files in (empty for the
/// in-memory backend).
fn new_holder(
    options: &BeekeeperOptions,
) -> (UniquePtr<ffi::BeekeeperHolder>, PathBuf) {
    if options.in_memory {
        let holder =
            ffi::new_beekeeper_holder_in_memory(options.unlock_timeout);
        return (holder, PathBuf::new());
    }

    let storage = new_rust_storage_protocol(&options.storage_root);
    let holder = ffi::new_beekeeper_holder(storage, options.unlock_timeout);
    let wallet_dir = Path::new(&options.storage_root).join(LEGACY_WALLET_DIR);

    (holder, wallet_dir)
}
