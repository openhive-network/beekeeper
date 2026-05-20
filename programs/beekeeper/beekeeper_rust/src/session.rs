use std::time::SystemTime;

use crate::{
    api::BeekeeperApi,
    errors::BeekeeperError,
    ffi,
    wallet::{LockedWallet, UnlockedWallet, WalletCreated},
};

pub struct Session<'a> {
    pub(super) bk: &'a mut BeekeeperApi,
    pub(super) token: &'a str,
}

pub struct SessionInfo {
    pub now: SystemTime,
    pub timeout_time: SystemTime,
}

impl<'a> Session<'a> {
    pub fn get_info(&self) -> SessionInfo {
        SessionInfo {
            now: SystemTime::now(),
            timeout_time: self.bk.get_timeout_time(),
        }
    }

    pub fn has_wallet(&self, name: &str) -> Result<bool, BeekeeperError> {
        Ok(self.bk.holder.has_wallet(name)?)
    }

    pub fn list_wallets(
        &self,
    ) -> Result<Vec<ffi::WalletDetails>, BeekeeperError> {
        Ok(self.bk.holder.list_wallets(self.token)?)
    }

    pub fn list_created_wallets(&self) -> Vec<String> {
        self.bk.list_created_wallets()
    }

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

    pub fn open_wallet(
        self,
        name: &str,
    ) -> Result<LockedWallet<'a>, BeekeeperError> {
        self.bk.holder.pin_mut().open_wallet(self.token, name)?;

        Ok(LockedWallet {
            bk: self.bk,
            token: self.token.to_string(),
            name: name.to_string(),
            is_temporary: false,
        })
    }

    pub fn close_wallet(&mut self, name: &str) -> Result<(), BeekeeperError> {
        self.bk.holder.pin_mut().close_wallet(name)?;
        Ok(())
    }

    pub fn lock_all(&mut self) -> Result<(), BeekeeperError> {
        let wallets = self.bk.holder.list_wallets(self.token)?;
        for wd in wallets.iter().filter(|w| w.unlocked) {
            self.bk.holder.pin_mut().lock(&wd.name)?;
        }

        Ok(())
    }

    pub fn close(mut self) -> Result<(), BeekeeperError> {
        self.lock_all()?;
        self.bk.close_session(self.token)?;

        Ok(())
    }
}
