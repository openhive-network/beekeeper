use crate::{api::BeekeeperApi, errors::BeekeeperError};

pub const DEFAULT_KEY_PREFIX: &str = "STM";

pub struct WalletInfo {
    pub name: String,
    pub unlocked: bool,
    pub is_temporary: bool,
}

pub struct WalletCreated<'a> {
    pub wallet: UnlockedWallet<'a>,
    pub password: String,
}

pub struct LockedWallet<'a> {
    pub(crate) bk: &'a mut BeekeeperApi,
    pub(crate) token: String,
    pub(crate) name: String,
    pub(crate) is_temporary: bool,
}

impl<'a> LockedWallet<'a> {
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

    pub fn close(self) -> Result<(), BeekeeperError> {
        self.bk.holder.pin_mut().close_wallet(&self.name)?;
        Ok(())
    }
}

pub struct UnlockedWallet<'a> {
    pub(crate) bk: &'a mut BeekeeperApi,
    pub(crate) token: String,
    pub(crate) name: String,
    pub(crate) is_temporary: bool,
}

impl<'a> UnlockedWallet<'a> {
    pub fn lock(self) -> Result<LockedWallet<'a>, BeekeeperError> {
        self.bk.holder.pin_mut().lock(&self.name)?;

        Ok(LockedWallet {
            bk: self.bk,
            token: self.token,
            name: self.name,
            is_temporary: self.is_temporary,
        })
    }

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

    pub fn get_public_keys(&mut self) -> Result<Vec<String>, BeekeeperError> {
        self.bk.throw_if_timed_out_and_refresh()?;

        Ok(self
            .bk
            .holder
            .get_public_keys(&self.name, DEFAULT_KEY_PREFIX)?)
    }

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

    pub fn close(self) -> Result<(), BeekeeperError> {
        let locked = self.lock()?;
        locked.close()
    }
}
