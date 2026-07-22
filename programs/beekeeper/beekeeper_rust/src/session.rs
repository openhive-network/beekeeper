//! Per-session wallet operations.
//!
//! A [`Session`] is an owned guard returned by
//! [`BeekeeperApi::create_session`](crate::api::BeekeeperApi::create_session);
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
/// handles), so multiple sessions can be alive at once. Dropping the guard
/// closes the session; there is no explicit `close` call.
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
    /// Does not mutate the activity clock.
    pub fn get_info(&self) -> SessionInfo {
        SessionInfo {
            now: SystemTime::now(),
            timeout_time: self.inner.borrow().get_timeout_time(),
        }
    }

    /// `true` when a wallet with the given name exists in the underlying
    /// wallet manager.
    ///
    /// The check is process-wide, not session-scoped: any wallet known to
    /// the wallet manager counts, whether or not this session opened it.
    pub fn has_wallet(&self, name: &str) -> Result<bool, BeekeeperError> {
        Ok(self.inner.borrow().holder.has_wallet(name)?)
    }

    /// All wallets currently opened in this session (locked or unlocked),
    /// enriched with their `is_temporary` flag.
    pub fn list_wallets(&self) -> Result<Vec<WalletInfo>, BeekeeperError> {
        let inner = self.inner.borrow();
        let details = inner.holder.list_wallets(&self.token)?;

        Ok(details
            .into_iter()
            .map(|wd| self.wallet_info(&inner, wd.name, wd.unlocked))
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
            .map(|name| self.wallet_info(&inner, name, false))
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
    /// flag.
    pub fn create_wallet<'p>(
        &self,
        name: &str,
        password: impl Into<Option<&'p str>>,
    ) -> Result<CreatedWallet, BeekeeperError> {
        let password = password.into();
        let (is_temp, generated_pw) = self.create_on_holder(name, password)?;
        let wallet = UnlockedWallet::new(
            Rc::clone(&self.inner),
            name.to_string(),
            is_temp,
        );

        Ok(CreatedWallet {
            wallet,
            password: password.map_or(generated_pw, str::to_string),
        })
    }

    /// Open an existing wallet and return its [`LockedWallet`] handle.
    ///
    /// The wallet is registered with `is_temporary: false` because
    /// previously-persisted wallets are by definition non-temporary.
    /// Opening an already-open wallet is a no-op.
    pub fn open_wallet(
        &self,
        name: &str,
    ) -> Result<LockedWallet, BeekeeperError> {
        {
            let mut inner = self.inner.borrow_mut();
            inner.holder.pin_mut().open_wallet(&self.token, name)?;
            inner.register_wallet(&self.token, name, false);
        }

        Ok(LockedWallet::new(
            Rc::clone(&self.inner),
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
    /// (post-lock) wallet listing, with every `unlocked` field `false`.
    ///
    /// Wallets opened via a different handle of the same session are also
    /// locked.
    pub fn lock_all(&self) -> Result<Vec<WalletInfo>, BeekeeperError> {
        let mut inner = self.inner.borrow_mut();
        let wallets = inner.lock_session_wallets(&self.token)?;

        Ok(wallets
            .into_iter()
            .map(|wd| self.wallet_info(&inner, wd.name, false))
            .collect())
    }

    /// Ask the C++ holder to create the wallet and register its metadata.
    ///
    /// Returns the applied `is_temporary` flag and the holder-generated
    /// password (meaningful when the caller passed no password).
    fn create_on_holder(
        &self,
        name: &str,
        password: Option<&str>,
    ) -> Result<(bool, String), BeekeeperError> {
        let mut inner = self.inner.borrow_mut();
        let is_temp = self.is_temporary.unwrap_or(inner.is_in_memory);
        let generated_pw = inner.holder.pin_mut().create_wallet(
            &self.token,
            name,
            password.unwrap_or(""),
            is_temp,
        )?;
        inner.register_wallet(&self.token, name, is_temp);

        Ok((is_temp, generated_pw))
    }

    /// Combine a holder-side listing entry with the Rust-tracked
    /// `is_temporary` flag.
    fn wallet_info(
        &self,
        inner: &Inner,
        name: String,
        unlocked: bool,
    ) -> WalletInfo {
        WalletInfo {
            is_temporary: inner.wallet_is_temporary(&self.token, &name),
            name,
            unlocked,
        }
    }
}

/// Locks everything in the session, then closes the session itself.
///
/// Errors are ignored: the session may already have been closed by
/// [`BeekeeperApi::delete`](crate::api::BeekeeperApi::delete), in which
/// case this is a no-op.
impl Drop for Session {
    fn drop(&mut self) {
        self.inner.borrow_mut().close_session_quietly(&self.token);
    }
}
