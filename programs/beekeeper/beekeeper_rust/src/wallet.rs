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

use crate::{api::BeekeeperApi, errors::BeekeeperError};

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
pub struct WalletCreated<'a> {
    pub wallet: UnlockedWallet<'a>,
    pub password: String,
}

/// A wallet known to the C++ holder but currently locked.
///
/// Borrows the same `&mut BeekeeperApi` as the parent
/// [`Session`](crate::session::Session) — once you hold a `LockedWallet`
/// you cannot start another `session()` borrow until you drop it.
pub struct LockedWallet<'a> {
    pub(crate) bk: &'a mut BeekeeperApi,
    pub(crate) token: String,
    pub(crate) name: String,
    pub(crate) is_temporary: bool,
}

impl<'a> LockedWallet<'a> {
    /// Unlock with the given password.
    ///
    /// Consumes `self` and returns an [`UnlockedWallet`]. Also refreshes
    /// the activity clock so the freshly-unlocked wallet has a full
    /// timeout window before any operation runs.
    pub fn unlock(
        self,
        password: &str,
    ) -> Result<UnlockedWallet<'a>, BeekeeperError> {
        self.bk.holder.pin_mut().unlock(&self.name, password)?;
        self.bk.refresh_timeout();

        Ok(UnlockedWallet {
            bk: self.bk,
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
        self.bk.holder.pin_mut().close_wallet(&self.name)?;
        Ok(())
    }
}

/// A wallet that is currently unlocked and ready to perform key operations.
///
/// Every method here first calls
/// [`throw_if_timed_out_and_refresh`](BeekeeperApi::throw_if_timed_out_and_refresh)
/// so that operations issued after the inactivity timeout elapsed fail
/// with [`BeekeeperError::TimedOut`] instead of silently succeeding.
pub struct UnlockedWallet<'a> {
    pub(crate) bk: &'a mut BeekeeperApi,
    pub(crate) token: String,
    pub(crate) name: String,
    pub(crate) is_temporary: bool,
}

impl<'a> UnlockedWallet<'a> {
    /// Lock the wallet and return its [`LockedWallet`] form.
    ///
    /// Does **not** check the inactivity timeout — locking is always
    /// allowed, even (especially) if the wallet has already auto-locked
    /// underneath us.
    pub fn lock(self) -> Result<LockedWallet<'a>, BeekeeperError> {
        self.bk.holder.pin_mut().lock(&self.name)?;

        Ok(LockedWallet {
            bk: self.bk,
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
        self.bk.throw_if_timed_out_and_refresh()?;

        Ok(self.bk.holder.pin_mut().import_key(
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
        self.bk.throw_if_timed_out_and_refresh()?;
        self.bk.holder.pin_mut().remove_key(
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
        self.bk.throw_if_timed_out_and_refresh()?;

        Ok(self.bk.holder.has_matching_private_key(
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
        self.bk.throw_if_timed_out_and_refresh()?;

        Ok(self.bk.holder.pin_mut().sign_digest(
            &self.name,
            digest_hex,
            public_key,
            DEFAULT_KEY_PREFIX,
        )?)
    }

    /// Every public key held by the wallet, formatted with
    /// [`DEFAULT_KEY_PREFIX`].
    pub fn get_public_keys(&mut self) -> Result<Vec<String>, BeekeeperError> {
        self.bk.throw_if_timed_out_and_refresh()?;

        Ok(self
            .bk
            .holder
            .get_public_keys(&self.name, DEFAULT_KEY_PREFIX)?)
    }

    /// Encrypt `content` between two keys, returning a base58 buffer.
    ///
    /// `from_key` must be present in the wallet (its private key is used
    /// for the ECDH side). `to_key = None` is interpreted as self-encryption
    /// (`to_key = from_key`), matching TS semantics.
    ///
    /// `nonce` is mixed into the encryption to make output reproducible;
    /// pass `0` if you don't care about reproducibility — the C++ side
    /// will generate a random nonce in that case (this matches the TS
    /// behaviour where the `?? 0` default is treated as "generate one").
    pub fn encrypt_data(
        &mut self,
        from_key: &str,
        to_key: Option<&str>,
        content: &str,
        nonce: u64,
    ) -> Result<String, BeekeeperError> {
        self.bk.throw_if_timed_out_and_refresh()?;
        Ok(self.bk.holder.pin_mut().encrypt_data(
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
    pub fn decrypt_data(
        &mut self,
        from_key: &str,
        to_key: Option<&str>,
        encrypted_content: &str,
    ) -> Result<String, BeekeeperError> {
        self.bk.throw_if_timed_out_and_refresh()?;
        Ok(self.bk.holder.pin_mut().decrypt_data(
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
