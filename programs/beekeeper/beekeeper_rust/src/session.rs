//! Per-session wallet operations.
//!
//! Counterpart of `BeekeeperSession` in
//! `beekeeper_wasm/src/detailed/session.ts`. A [`Session`] is an owned guard
//! returned by [`BeekeeperApi::create_session`](crate::api::BeekeeperApi::create_session);
//! it closes itself (locking its wallets first) when dropped.

use std::{cell::RefCell, rc::Rc, time::SystemTime};

use crate::{
    api::Inner,
    errors::BeekeeperError,
    wallet::{CreatedWallet, LockedWallet, UnlockedWallet, WalletInfo},
};

/// Owned guard over one beekeeper session.
///
/// Sessions don't own the underlying C++ state — they share it with the
/// parent [`BeekeeperApi`](crate::api::BeekeeperApi) (and with any wallet
/// handles), so multiple sessions can be alive at once.
///
/// # Difference from TS
///
/// - TS `BeekeeperSession` keeps its own `wallets: Map<string, ...>` and is
///   the source of truth for "which wallets does this session own". Rust
///   stores only `WalletMeta` on the shared state; the C++ holder is asked
///   directly about wallet membership through `list_wallets`.
/// - TS requires an explicit `session.close()`. Rust closes the session in
///   `Drop` — just let the guard go out of scope.
pub struct Session {
    inner: Rc<RefCell<Inner>>,
    token: String,
    is_temporary: Option<bool>,
}

/// Snapshot returned by [`Session::get_info`].
///
/// `now` is the wall-clock at the moment the call was made; `timeout_time`
/// is the wall-clock instant at which the wallet manager will auto-lock
/// (or far-future if the timeout is disabled).
pub struct SessionInfo {
    pub now: SystemTime,
    pub timeout_time: SystemTime,
}

impl Session {
    pub(crate) fn new(inner: Rc<RefCell<Inner>>, token: String) -> Self {
        Self {
            inner,
            token,
            is_temporary: None,
        }
    }

    /// Builder-style override for the `is_temporary` flag applied by
    /// [`create_wallet`](Self::create_wallet).
    ///
    /// When never called, wallets default to temporary if and only if the
    /// beekeeper was opened with
    /// [`BeekeeperOptions::in_memory`](crate::options::BeekeeperOptions::in_memory).
    ///
    /// # Difference from TS
    ///
    /// TS passes `isTemporary` as the third `createWallet` argument. Rust
    /// configures it once on the session, builder-style (like
    /// [`BeekeeperOptions::in_memory`](crate::options::BeekeeperOptions::in_memory)):
    ///
    /// ```no_run
    /// # use beekeeper::prelude::*;
    /// # let bk = BeekeeperApi::new(None);
    /// let session = bk.create_session().unwrap().is_temporary(true);
    /// let created = session.create_wallet("scratch", "password").unwrap();
    /// ```
    pub fn is_temporary(mut self, is_temporary: bool) -> Self {
        self.is_temporary = Some(is_temporary);
        self
    }

    /// Capture a [`SessionInfo`] snapshot.
    ///
    /// Equivalent of TS `getInfo()`. Does not mutate the activity clock.
    pub fn get_info(&self) -> SessionInfo {
        SessionInfo {
            now: SystemTime::now(),
            timeout_time: self.inner.borrow().get_timeout_time(),
        }
    }

    /// `true` when a wallet with the given name exists in the underlying
    /// C++ wallet manager.
    ///
    /// # Difference from TS
    ///
    /// TS calls `api.has_wallet(token, name)` (token-scoped). The C++
    /// holder used here only takes a name, so the result is process-wide
    /// rather than session-scoped.
    pub fn has_wallet(&self, name: &str) -> Result<bool, BeekeeperError> {
        Ok(self.inner.borrow().holder.has_wallet(name)?)
    }

    /// All wallets currently opened in this session (locked or unlocked),
    /// enriched with their `is_temporary` flag.
    ///
    /// # Difference from TS
    ///
    /// TS returns the contents of the session's local `wallets` `Map`.
    /// Rust queries the C++ holder for the canonical list and then merges
    /// in `is_temporary` from the Rust-side metadata.
    pub fn list_wallets(&self) -> Result<Vec<WalletInfo>, BeekeeperError> {
        let inner = self.inner.borrow();
        let details = inner.holder.list_wallets(&self.token)?;
        Ok(details
            .into_iter()
            .map(|wd| WalletInfo {
                is_temporary: inner.wallet_is_temporary(&self.token, &wd.name),
                name: wd.name,
                unlocked: wd.unlocked,
            })
            .collect())
    }

    /// All wallets present on disk, opened or not.
    ///
    /// Each entry is reported as `unlocked: false`; the `is_temporary` flag
    /// is filled in from this session's metadata if the wallet was created
    /// or opened through this session in the past, otherwise defaults to
    /// `false`.
    ///
    /// In in-memory mode this returns an empty vector.
    pub fn list_created_wallets(&self) -> Vec<WalletInfo> {
        let inner = self.inner.borrow();
        inner
            .list_created_wallets()
            .into_iter()
            .map(|name| WalletInfo {
                is_temporary: inner.wallet_is_temporary(&self.token, &name),
                unlocked: false,
                name,
            })
            .collect()
    }

    /// Create a brand-new wallet and return it as a [`CreatedWallet`].
    ///
    /// `password` accepts anything convertible into `Option<&str>` — pass a
    /// `&str` directly, or `None` to have the C++ side generate a random
    /// password, returned in the [`CreatedWallet::password`] field.
    ///
    /// Whether the wallet is temporary is controlled by the
    /// [`is_temporary`](Self::is_temporary) builder; unset, it defaults to
    /// the beekeeper-wide
    /// [`BeekeeperOptions::in_memory`](crate::options::BeekeeperOptions::in_memory)
    /// flag, matching the TS semantics.
    pub fn create_wallet<'p>(
        &self,
        name: &str,
        password: impl Into<Option<&'p str>>,
    ) -> Result<CreatedWallet, BeekeeperError> {
        let password = password.into();
        let mut inner = self.inner.borrow_mut();
        let is_temp = self.is_temporary.unwrap_or(inner.is_in_memory);

        let returned_pw = inner.holder.pin_mut().create_wallet(
            &self.token,
            name,
            password.unwrap_or(""),
            is_temp,
        )?;
        inner.register_wallet(&self.token, name, is_temp);
        let pw = match password {
            Some(p) => p.to_string(),
            None => returned_pw,
        };
        drop(inner);

        Ok(CreatedWallet {
            wallet: UnlockedWallet::new(
                Rc::clone(&self.inner),
                self.token.clone(),
                name.to_string(),
                is_temp,
            ),
            password: pw,
        })
    }

    /// Open an existing wallet and return its [`LockedWallet`] handle.
    ///
    /// The wallet is registered with `is_temporary: false` because
    /// previously-persisted wallets are by definition non-temporary.
    ///
    /// # Difference from TS
    ///
    /// TS first checks the session's local `wallets` map and short-circuits
    /// if the wallet is already open. Rust unconditionally asks the C++
    /// holder to open it; opening an already-open wallet is a no-op there.
    pub fn open_wallet(
        &self,
        name: &str,
    ) -> Result<LockedWallet, BeekeeperError> {
        let mut inner = self.inner.borrow_mut();
        inner.holder.pin_mut().open_wallet(&self.token, name)?;
        inner.register_wallet(&self.token, name, false);
        drop(inner);

        Ok(LockedWallet::new(
            Rc::clone(&self.inner),
            self.token.clone(),
            name.to_string(),
            false,
        ))
    }

    /// Close `name` on the C++ side and drop its metadata entry.
    ///
    /// This does **not** lock the wallet first — call
    /// [`UnlockedWallet::lock`](crate::wallet::UnlockedWallet::lock) yourself
    /// if the wallet is unlocked. The higher-level [`LockedWallet::close`]
    /// and [`UnlockedWallet::close`](crate::wallet::UnlockedWallet::close)
    /// methods do the right thing automatically.
    pub fn close_wallet(&self, name: &str) -> Result<(), BeekeeperError> {
        let mut inner = self.inner.borrow_mut();
        inner.holder.pin_mut().close_wallet(name)?;
        inner.unregister_wallet(&self.token, name);
        Ok(())
    }

    /// Lock every unlocked wallet in this session and return the resulting
    /// (post-lock) wallet listing.
    ///
    /// Iterates the listing reported by the C++ holder and locks each entry
    /// whose `unlocked` flag is true. The returned vector reflects the
    /// new state with every `unlocked` field set to `false`.
    ///
    /// # Difference from TS
    ///
    /// TS calls `wallet.unlocked.lock()` on each opened wallet in the
    /// session's local map. Rust drives the loop from the C++ holder's
    /// listing instead, so wallets that were opened via a different handle
    /// of the same session are also locked.
    pub fn lock_all(&self) -> Result<Vec<WalletInfo>, BeekeeperError> {
        let mut inner = self.inner.borrow_mut();
        let wallets = inner.holder.list_wallets(&self.token)?;
        for wd in wallets.iter().filter(|w| w.unlocked) {
            inner.holder.pin_mut().lock(&wd.name)?;
        }

        Ok(wallets
            .into_iter()
            .map(|wd| WalletInfo {
                is_temporary: inner.wallet_is_temporary(&self.token, &wd.name),
                name: wd.name,
                unlocked: false,
            })
            .collect())
    }
}

/// Locks everything in the session, then closes the session itself —
/// the equivalent of TS `BeekeeperSession.close()`, run automatically.
///
/// Errors are ignored: the session may already have been closed by
/// [`BeekeeperApi::delete`](crate::api::BeekeeperApi::delete), in which
/// case this is a no-op.
impl Drop for Session {
    fn drop(&mut self) {
        let mut inner = self.inner.borrow_mut();
        if !inner.session_is_open(&self.token) {
            return;
        }
        if let Ok(wallets) = inner.holder.list_wallets(&self.token) {
            for wd in wallets.iter().filter(|w| w.unlocked) {
                let _ = inner.holder.pin_mut().lock(&wd.name);
            }
        }
        let _ = inner.close_session(&self.token);
    }
}
