use crate::{api::BeekeeperApi, errors::BeekeeperError};

pub const DEFAULT_KEY_PREFIX: &str = "STM";

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

    pub fn close(self) -> Result<(), BeekeeperError> {
        let locked = self.lock()?;
        locked.close()
    }
}
