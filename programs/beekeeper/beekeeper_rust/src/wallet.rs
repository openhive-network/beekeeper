//! Locked / unlocked wallet handles.
//!
//! Counterpart of `BeekeeperLockedWallet` / `BeekeeperUnlockedWallet` in
//! `beekeeper_wasm/src/detailed/wallet.ts`.
//!
//! # Difference from TS
//!
//! TS represents lock state with a single `BeekeeperLockedWallet` whose
//! optional `_unlocked` field is set/cleared as the wallet changes state.
//! Rust models it as two distinct types ([`LockedWallet`] and
//! [`UnlockedWallet`]) and makes the transition methods consume `self`, so
//! the type system enforces "you can only call `import_key` on an
//! unlocked wallet" instead of relying on a runtime check.

use std::{
    cell::RefCell,
    ops::{Deref, DerefMut},
    rc::Rc,
};

use crate::{api::Inner, errors::BeekeeperError};

/// Address-format prefix used for every Hive public key serialised by the
/// C++ side ("STM…").
///
/// # Difference from TS
///
/// Both bindings hard-code this prefix; the C++ ABI accepts a prefix
/// parameter for future flexibility but neither facade exposes it.
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
/// the caller-supplied value, or the C++-generated one when `None` was
/// passed.
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

/// A wallet known to the C++ holder but currently locked.
///
/// Shares the underlying state with the parent
/// [`Session`](crate::session::Session) — the handle stays valid for as
/// long as the session is alive.
pub struct LockedWallet {
    inner: Rc<RefCell<Inner>>,
    token: String,
    name: String,
    is_temporary: bool,
}

impl LockedWallet {
    pub(crate) fn new(
        inner: Rc<RefCell<Inner>>,
        token: String,
        name: String,
        is_temporary: bool,
    ) -> Self {
        Self {
            inner,
            token,
            name,
            is_temporary,
        }
    }

    /// The wallet's name, as passed to `create_wallet` / `open_wallet`.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// `true` when the wallet lives only in process memory and is never
    /// persisted. Equivalent of the TS `isTemporary` getter.
    pub fn is_temporary(&self) -> bool {
        self.is_temporary
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
            let mut inner = self.inner.borrow_mut();
            inner.holder.pin_mut().unlock(&self.name, password)?;
            inner.refresh_timeout();
        }

        Ok(UnlockedWallet {
            inner: self.inner,
            token: self.token,
            name: self.name,
            is_temporary: self.is_temporary,
        })
    }

    /// Close the wallet on the C++ side.
    ///
    /// Consumes `self`. The Rust-side metadata entry stays put — close the
    /// session if you want it gone, or call
    /// [`Session::close_wallet`](crate::session::Session::close_wallet)
    /// instead, which also unregisters the metadata.
    pub fn close(self) -> Result<(), BeekeeperError> {
        self.inner
            .borrow_mut()
            .holder
            .pin_mut()
            .close_wallet(&self.name)?;
        Ok(())
    }
}

/// A wallet that is currently unlocked and ready to perform key operations.
///
/// Every method here first calls the shared `throw_if_timed_out_and_refresh`
/// so that operations issued after the inactivity timeout elapsed fail
/// with [`BeekeeperError::TimedOut`] instead of silently succeeding.
pub struct UnlockedWallet {
    inner: Rc<RefCell<Inner>>,
    token: String,
    name: String,
    is_temporary: bool,
}

impl UnlockedWallet {
    pub(crate) fn new(
        inner: Rc<RefCell<Inner>>,
        token: String,
        name: String,
        is_temporary: bool,
    ) -> Self {
        Self {
            inner,
            token,
            name,
            is_temporary,
        }
    }

    /// The wallet's name, as passed to `create_wallet` / `open_wallet`.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// `true` when the wallet lives only in process memory and is never
    /// persisted. Equivalent of the TS `isTemporary` getter.
    pub fn is_temporary(&self) -> bool {
        self.is_temporary
    }

    /// Lock the wallet and return its [`LockedWallet`] form.
    ///
    /// Does **not** check the inactivity timeout — locking is always
    /// allowed, even (especially) if the wallet has already auto-locked
    /// underneath us.
    pub fn lock(self) -> Result<LockedWallet, BeekeeperError> {
        self.inner.borrow_mut().holder.pin_mut().lock(&self.name)?;

        Ok(LockedWallet {
            inner: self.inner,
            token: self.token,
            name: self.name,
            is_temporary: self.is_temporary,
        })
    }

    /// Import a WIF-encoded private key and return its public counterpart.
    ///
    /// Returned key is base58 with the [`DEFAULT_KEY_PREFIX`] prefix
    /// (e.g. `"STM7..."`).
    pub fn import_key(
        &mut self,
        wif_key: &str,
    ) -> Result<String, BeekeeperError> {
        let mut inner = self.inner.borrow_mut();
        inner.throw_if_timed_out_and_refresh()?;

        Ok(inner.holder.pin_mut().import_key(
            &self.name,
            wif_key,
            DEFAULT_KEY_PREFIX,
        )?)
    }

    /// Remove the private key matching `public_key` (WIF, `STM…`).
    ///
    /// Errors when no such key exists in the wallet.
    pub fn remove_key(
        &mut self,
        public_key: &str,
    ) -> Result<(), BeekeeperError> {
        let mut inner = self.inner.borrow_mut();
        inner.throw_if_timed_out_and_refresh()?;
        inner.holder.pin_mut().remove_key(
            &self.name,
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
        let mut inner = self.inner.borrow_mut();
        inner.throw_if_timed_out_and_refresh()?;

        Ok(inner.holder.has_matching_private_key(
            &self.name,
            public_key,
            DEFAULT_KEY_PREFIX,
        )?)
    }

    /// Sign a pre-computed digest with the private key matching `public_key`.
    ///
    /// `digest_hex` must be the hex encoding of a 32-byte digest. The
    /// returned signature is the hex encoding of the fc 65-byte compact
    /// signature.
    ///
    /// # Difference from TS
    ///
    /// TS accepts `string | Uint8Array` and hex-encodes the byte array
    /// internally. Rust takes a hex string only; encode upstream if you
    /// have raw bytes.
    pub fn sign_digest(
        &mut self,
        public_key: &str,
        digest_hex: &str,
    ) -> Result<String, BeekeeperError> {
        let mut inner = self.inner.borrow_mut();
        inner.throw_if_timed_out_and_refresh()?;

        Ok(inner.holder.pin_mut().sign_digest(
            &self.name,
            digest_hex,
            public_key,
            DEFAULT_KEY_PREFIX,
        )?)
    }

    /// Every public key held by the wallet, formatted with
    /// [`DEFAULT_KEY_PREFIX`].
    pub fn get_public_keys(&mut self) -> Result<Vec<String>, BeekeeperError> {
        let mut inner = self.inner.borrow_mut();
        inner.throw_if_timed_out_and_refresh()?;

        Ok(inner.holder.get_public_keys(&self.name, DEFAULT_KEY_PREFIX)?)
    }

    /// Encrypt `content` between two keys, returning a base58 buffer.
    ///
    /// `from_key` must be present in the wallet (its private key is used
    /// for the ECDH side). `to_key` accepts anything convertible into
    /// `Option<&str>`; `None` is interpreted as self-encryption
    /// (`to_key = from_key`), matching TS semantics.
    ///
    /// `nonce` is mixed into the encryption to make output reproducible;
    /// pass `0` if you don't care about reproducibility — the C++ side
    /// will generate a random nonce in that case (this matches the TS
    /// behaviour where the `?? 0` default is treated as "generate one").
    pub fn encrypt_data<'k>(
        &mut self,
        from_key: &str,
        to_key: impl Into<Option<&'k str>>,
        content: &str,
        nonce: u64,
    ) -> Result<String, BeekeeperError> {
        let to_key = to_key.into();
        let mut inner = self.inner.borrow_mut();
        inner.throw_if_timed_out_and_refresh()?;
        Ok(inner.holder.pin_mut().encrypt_data(
            &self.name,
            from_key,
            to_key.unwrap_or(from_key),
            content,
            DEFAULT_KEY_PREFIX,
            nonce,
        )?)
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
        let to_key = to_key.into();
        let mut inner = self.inner.borrow_mut();
        inner.throw_if_timed_out_and_refresh()?;
        Ok(inner.holder.pin_mut().decrypt_data(
            &self.name,
            from_key,
            to_key.unwrap_or(from_key),
            encrypted_content,
            DEFAULT_KEY_PREFIX,
        )?)
    }

    /// Lock then close the wallet.
    ///
    /// Equivalent of TS `BeekeeperUnlockedWallet.close()`.
    pub fn close(self) -> Result<(), BeekeeperError> {
        let locked = self.lock()?;
        locked.close()
    }
}
