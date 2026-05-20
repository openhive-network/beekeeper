//! Per-session wallet operations.
//!
//! Counterpart of `BeekeeperSession` in
//! `beekeeper_wasm/src/detailed/session.ts`. A `Session` is a short-lived
//! borrow over a [`BeekeeperApi`] scoped to a single session token, returned
//! by [`BeekeeperApi::session`](crate::api::BeekeeperApi::session).

use std::time::SystemTime;

use crate::{
    api::BeekeeperApi,
    errors::BeekeeperError,
    wallet::{LockedWallet, UnlockedWallet, WalletCreated, WalletInfo},
};

/// Borrowed view of a [`BeekeeperApi`] scoped to one session token.
///
/// Sessions don't own the underlying C++ state — they are just a typed
/// reference. The struct holds a `&mut BeekeeperApi`, so the borrow checker
/// guarantees that at most one `Session` is alive at a time and that the
/// token outlives the borrow.
///
/// # Difference from TS
///
/// In TS `BeekeeperSession` keeps its own `wallets: Map<string, ...>` and
/// is the source of truth for "which wallets does this session own". Rust
/// stores only `WalletMeta` on the parent [`BeekeeperApi`]; the C++ holder
/// is asked directly about wallet membership through `list_wallets`.
pub struct Session<'a> {
    pub(super) bk: &'a mut BeekeeperApi,
    pub(super) token: &'a str,
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

impl<'a> Session<'a> {
    /// Capture a [`SessionInfo`] snapshot.
    ///
    /// Equivalent of TS `getInfo()`. Does not mutate the activity clock.
    pub fn get_info(&self) -> SessionInfo {
        SessionInfo {
            now: SystemTime::now(),
            timeout_time: self.bk.get_timeout_time(),
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
        Ok(self.bk.holder.has_wallet(name)?)
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
        let details = self.bk.holder.list_wallets(self.token)?;
        Ok(details
            .into_iter()
            .map(|wd| WalletInfo {
                is_temporary: self.bk.wallet_is_temporary(self.token, &wd.name),
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
        self.bk
            .list_created_wallets()
            .into_iter()
            .map(|name| WalletInfo {
                is_temporary: self.bk.wallet_is_temporary(self.token, &name),
                unlocked: false,
                name,
            })
            .collect()
    }

    /// Create a brand-new wallet and return its [`UnlockedWallet`] handle.
    ///
    /// - `password = None` asks the C++ side to generate a random password,
    ///   which is returned in the [`WalletCreated::password`] field.
    /// - `is_temporary = None` defaults to the beekeeper-wide
    ///   [`BeekeeperOptions::in_memory`](crate::options::BeekeeperOptions::in_memory)
    ///   flag, matching the TS semantics.
    ///
    /// This consumes the `Session` because the returned wallet borrows
    /// the same `&mut BeekeeperApi`; reach for another `session()` call
    /// when you're done with the wallet.
    ///
    /// # Difference from TS
    ///
    /// TS exposes two overloaded `createWallet` signatures
    /// (`(name, password?)` and `(name, password|undefined, isTemporary?)`).
    /// Rust always takes both optional parameters explicitly.
    pub fn create_wallet(
        self,
        name: &str,
        password: Option<&str>,
        is_temporary: Option<bool>,
    ) -> Result<WalletCreated<'a>, BeekeeperError> {
        let is_temp = is_temporary.unwrap_or(self.bk.is_in_memory);

        let returned_pw = self.bk.holder.pin_mut().create_wallet(
            self.token,
            name,
            password.unwrap_or(""),
            is_temp,
        )?;
        self.bk.register_wallet(self.token, name, is_temp);
        let pw = match password {
            Some(p) => p.to_string(),
            None => returned_pw,
        };

        Ok(WalletCreated {
            wallet: UnlockedWallet {
                bk: self.bk,
                token: self.token.to_string(),
                name: name.to_string(),
                is_temporary: is_temp,
            },
            password: pw,
        })
    }

    /// Open an existing wallet and return its [`LockedWallet`] handle.
    ///
    /// The wallet is registered with `is_temporary: false` because
    /// previously-persisted wallets are by definition non-temporary.
    ///
    /// Consumes the `Session` — see [`create_wallet`](Self::create_wallet)
    /// for the rationale.
    ///
    /// # Difference from TS
    ///
    /// TS first checks the session's local `wallets` map and short-circuits
    /// if the wallet is already open. Rust unconditionally asks the C++
    /// holder to open it; opening an already-open wallet is a no-op there.
    pub fn open_wallet(
        self,
        name: &str,
    ) -> Result<LockedWallet<'a>, BeekeeperError> {
        self.bk.holder.pin_mut().open_wallet(self.token, name)?;
        self.bk.register_wallet(self.token, name, false);

        Ok(LockedWallet {
            bk: self.bk,
            token: self.token.to_string(),
            name: name.to_string(),
            is_temporary: false,
        })
    }

    /// Close `name` on the C++ side and drop its metadata entry.
    ///
    /// This does **not** lock the wallet first — call
    /// [`UnlockedWallet::lock`](crate::wallet::UnlockedWallet::lock) yourself
    /// if the wallet is unlocked. The higher-level [`LockedWallet::close`]
    /// and [`UnlockedWallet::close`](crate::wallet::UnlockedWallet::close)
    /// methods do the right thing automatically.
    pub fn close_wallet(&mut self, name: &str) -> Result<(), BeekeeperError> {
        self.bk.holder.pin_mut().close_wallet(name)?;
        self.bk.unregister_wallet(self.token, name);
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
    /// listing instead, so wallets that were opened outside this `Session`
    /// instance (e.g. via a different `session()` borrow earlier) are also
    /// locked.
    pub fn lock_all(&mut self) -> Result<Vec<WalletInfo>, BeekeeperError> {
        let wallets = self.bk.holder.list_wallets(self.token)?;
        for wd in wallets.iter().filter(|w| w.unlocked) {
            self.bk.holder.pin_mut().lock(&wd.name)?;
        }

        Ok(wallets
            .into_iter()
            .map(|wd| WalletInfo {
                is_temporary: self.bk.wallet_is_temporary(self.token, &wd.name),
                name: wd.name,
                unlocked: false,
            })
            .collect())
    }

    /// Lock everything in the session, then close the session itself.
    ///
    /// Equivalent of TS `BeekeeperSession.close()`. Consumes the `Session`.
    pub fn close(mut self) -> Result<(), BeekeeperError> {
        self.lock_all()?;
        self.bk.close_session(self.token)?;

        Ok(())
    }
}
