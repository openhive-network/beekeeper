//! Locked / unlocked wallet handles.
//!
//! Lock state is encoded in the type system: [`LockedWallet`] and
//! [`UnlockedWallet`] are distinct types, and every transition
//! (`unlock` / `lock` / `close`) consumes its receiver and returns the next
//! state. Calling a key operation on a locked wallet is a compile error,
//! not a runtime check.

use std::{
    cell::{RefCell, RefMut},
    ops::{Deref, DerefMut},
    rc::Rc,
};

use crate::{api::Inner, errors::BeekeeperError};

/// Address-format prefix used for every Hive public key serialised by the
/// C++ side ("STM…").
pub const DEFAULT_KEY_PREFIX: &str = "STM";

/// Plain summary of a wallet returned from listing operations.
///
/// Combines the C++ holder's `(name, unlocked)` data with the Rust-tracked
/// `is_temporary` flag.
pub struct WalletInfo {
    pub name: String,
    pub unlocked: bool,
    pub is_temporary: bool,
}

/// Result of [`Session::create_wallet`](crate::session::Session::create_wallet).
///
/// `password` is whichever password ended up protecting the wallet — either
/// the caller-supplied value, or the generated one when `None` was passed.
///
/// Dereferences to the contained [`UnlockedWallet`], so key operations can
/// be called on it directly:
///
/// ```no_run
/// # use beekeeper::prelude::*;
/// # let bk = BeekeeperApi::new(None);
/// # let session = bk.create_session().unwrap();
/// let mut created = session.create_wallet("w0", "password").unwrap();
/// let pubkey = created.import_key("5J...wif...").unwrap();
/// ```
pub struct CreatedWallet {
    pub wallet: UnlockedWallet,
    pub password: String,
}

/// A wallet known to the wallet manager but currently locked.
///
/// Shares the underlying state with the parent
/// [`Session`](crate::session::Session) — the handle stays valid for as
/// long as the session is alive.
pub struct LockedWallet {
    handle: WalletHandle,
}

/// A wallet that is currently unlocked and ready to perform key operations.
///
/// Every operation first checks the inactivity timeout, so calls issued
/// after it elapsed fail with [`BeekeeperError::TimedOut`] instead of
/// silently succeeding.
pub struct UnlockedWallet {
    handle: WalletHandle,
}

/// State common to both lock states: the wallet's identity plus access to
/// the beekeeper it belongs to. Transitions move the handle unchanged.
struct WalletHandle {
    inner: Rc<RefCell<Inner>>,
    name: String,
    is_temporary: bool,
}

impl CreatedWallet {
    /// Consume the pair, keeping only the wallet handle.
    pub fn into_wallet(self) -> UnlockedWallet {
        self.wallet
    }

    /// Forward of [`UnlockedWallet::lock`] (consuming methods cannot go
    /// through `Deref`).
    pub fn lock(self) -> Result<LockedWallet, BeekeeperError> {
        self.wallet.lock()
    }

    /// Forward of [`UnlockedWallet::close`].
    pub fn close(self) -> Result<(), BeekeeperError> {
        self.wallet.close()
    }
}

impl Deref for CreatedWallet {
    type Target = UnlockedWallet;

    fn deref(&self) -> &UnlockedWallet {
        &self.wallet
    }
}

impl DerefMut for CreatedWallet {
    fn deref_mut(&mut self) -> &mut UnlockedWallet {
        &mut self.wallet
    }
}

impl LockedWallet {
    pub(crate) fn new(
        inner: Rc<RefCell<Inner>>,
        name: String,
        is_temporary: bool,
    ) -> Self {
        Self {
            handle: WalletHandle::new(inner, name, is_temporary),
        }
    }

    /// The wallet's name, as passed to `create_wallet` / `open_wallet`.
    pub fn name(&self) -> &str {
        &self.handle.name
    }

    /// `true` when the wallet lives only in process memory and is never
    /// persisted.
    pub fn is_temporary(&self) -> bool {
        self.handle.is_temporary
    }

    /// Unlock with the given password.
    ///
    /// Consumes `self` and returns an [`UnlockedWallet`]. Also refreshes
    /// the activity clock so the freshly-unlocked wallet has a full
    /// timeout window before any operation runs.
    pub fn unlock(
        self,
        password: &str,
    ) -> Result<UnlockedWallet, BeekeeperError> {
        {
            let mut inner = self.handle.inner.borrow_mut();
            inner.holder.pin_mut().unlock(&self.handle.name, password)?;
            inner.refresh_timeout();
        }

        Ok(UnlockedWallet {
            handle: self.handle,
        })
    }

    /// Close the wallet on the C++ side.
    ///
    /// Consumes `self`. The Rust-side metadata entry stays put — close the
    /// session if you want it gone, or call
    /// [`Session::close_wallet`](crate::session::Session::close_wallet)
    /// instead, which also unregisters the metadata.
    pub fn close(self) -> Result<(), BeekeeperError> {
        let mut inner = self.handle.inner.borrow_mut();
        inner.holder.pin_mut().close_wallet(&self.handle.name)?;

        Ok(())
    }
}

impl UnlockedWallet {
    pub(crate) fn new(
        inner: Rc<RefCell<Inner>>,
        name: String,
        is_temporary: bool,
    ) -> Self {
        Self {
            handle: WalletHandle::new(inner, name, is_temporary),
        }
    }

    /// The wallet's name, as passed to `create_wallet` / `open_wallet`.
    pub fn name(&self) -> &str {
        &self.handle.name
    }

    /// `true` when the wallet lives only in process memory and is never
    /// persisted.
    pub fn is_temporary(&self) -> bool {
        self.handle.is_temporary
    }

    /// Lock the wallet and return its [`LockedWallet`] form.
    ///
    /// Does **not** check the inactivity timeout — locking is always
    /// allowed, even (especially) if the wallet has already auto-locked
    /// underneath us.
    pub fn lock(self) -> Result<LockedWallet, BeekeeperError> {
        {
            let mut inner = self.handle.inner.borrow_mut();
            inner.holder.pin_mut().lock(&self.handle.name)?;
        }

        Ok(LockedWallet {
            handle: self.handle,
        })
    }

    /// Lock then close the wallet.
    pub fn close(self) -> Result<(), BeekeeperError> {
        let locked = self.lock()?;
        locked.close()
    }

    /// Import a WIF-encoded private key and return its public counterpart.
    ///
    /// The returned key is base58 with the [`DEFAULT_KEY_PREFIX`] prefix
    /// (e.g. `"STM7..."`).
    pub fn import_key(
        &mut self,
        wif_key: &str,
    ) -> Result<String, BeekeeperError> {
        let mut inner = self.handle.refreshed()?;
        let public_key = inner.holder.pin_mut().import_key(
            &self.handle.name,
            wif_key,
            DEFAULT_KEY_PREFIX,
        )?;

        Ok(public_key)
    }

    /// Remove the private key matching `public_key` (`STM…`).
    ///
    /// Errors when no such key exists in the wallet.
    pub fn remove_key(
        &mut self,
        public_key: &str,
    ) -> Result<(), BeekeeperError> {
        let mut inner = self.handle.refreshed()?;
        inner.holder.pin_mut().remove_key(
            &self.handle.name,
            public_key,
            DEFAULT_KEY_PREFIX,
        )?;

        Ok(())
    }

    /// `true` when the wallet contains the private key for `public_key`.
    pub fn has_matching_private_key(
        &mut self,
        public_key: &str,
    ) -> Result<bool, BeekeeperError> {
        let inner = self.handle.refreshed()?;
        let has_key = inner.holder.has_matching_private_key(
            &self.handle.name,
            public_key,
            DEFAULT_KEY_PREFIX,
        )?;

        Ok(has_key)
    }

    /// Sign a pre-computed digest with the private key matching `public_key`.
    ///
    /// `digest_hex` must be the hex encoding of a 32-byte digest. The
    /// returned signature is the hex encoding of the 65-byte compact
    /// signature.
    pub fn sign_digest(
        &mut self,
        public_key: &str,
        digest_hex: &str,
    ) -> Result<String, BeekeeperError> {
        let mut inner = self.handle.refreshed()?;
        let signature = inner.holder.pin_mut().sign_digest(
            &self.handle.name,
            digest_hex,
            public_key,
            DEFAULT_KEY_PREFIX,
        )?;

        Ok(signature)
    }

    /// Every public key held by the wallet, formatted with
    /// [`DEFAULT_KEY_PREFIX`].
    pub fn get_public_keys(&mut self) -> Result<Vec<String>, BeekeeperError> {
        let inner = self.handle.refreshed()?;
        let keys = inner
            .holder
            .get_public_keys(&self.handle.name, DEFAULT_KEY_PREFIX)?;

        Ok(keys)
    }

    /// Encrypt `content` between two keys, returning a base58 buffer.
    ///
    /// `from_key` must be present in the wallet (its private key is used
    /// for the ECDH side). `to_key` accepts anything convertible into
    /// `Option<&str>`; `None` is interpreted as self-encryption
    /// (`to_key = from_key`).
    ///
    /// `nonce` is mixed into the encryption to make output reproducible;
    /// pass `0` to have the C++ side generate a random nonce instead.
    pub fn encrypt_data<'k>(
        &mut self,
        from_key: &str,
        to_key: impl Into<Option<&'k str>>,
        content: &str,
        nonce: u64,
    ) -> Result<String, BeekeeperError> {
        let to_key = to_key.into().unwrap_or(from_key);
        let mut inner = self.handle.refreshed()?;
        let encrypted = inner.holder.pin_mut().encrypt_data(
            &self.handle.name,
            from_key,
            to_key,
            content,
            DEFAULT_KEY_PREFIX,
            nonce,
        )?;

        Ok(encrypted)
    }

    /// Decrypt a base58 buffer produced by [`encrypt_data`](Self::encrypt_data).
    ///
    /// `to_key = None` defaults to `from_key`, mirroring the encrypt side.
    pub fn decrypt_data<'k>(
        &mut self,
        from_key: &str,
        to_key: impl Into<Option<&'k str>>,
        encrypted_content: &str,
    ) -> Result<String, BeekeeperError> {
        let to_key = to_key.into().unwrap_or(from_key);
        let mut inner = self.handle.refreshed()?;
        let decrypted = inner.holder.pin_mut().decrypt_data(
            &self.handle.name,
            from_key,
            to_key,
            encrypted_content,
            DEFAULT_KEY_PREFIX,
        )?;

        Ok(decrypted)
    }
}

impl WalletHandle {
    fn new(
        inner: Rc<RefCell<Inner>>,
        name: String,
        is_temporary: bool,
    ) -> Self {
        Self {
            inner,
            name,
            is_temporary,
        }
    }

    /// Borrow the shared state, failing with [`BeekeeperError::TimedOut`]
    /// if the inactivity timeout has elapsed (and refreshing it otherwise).
    fn refreshed(&self) -> Result<RefMut<'_, Inner>, BeekeeperError> {
        let mut inner = self.inner.borrow_mut();
        inner.throw_if_timed_out_and_refresh()?;

        Ok(inner)
    }
}
