use std::time::SystemTime;

use crate::{api::BeekeeperApi, errors::BeekeeperError};

pub struct SessionInfo {
    pub now: SystemTime,
    pub timeout_time: SystemTime,
}

pub struct Session<'a> {
    pub(super) bk: &'a mut BeekeeperApi,
    pub(super) token: &'a str,
}

impl<'a> Session<'a> {
    pub fn token(&self) -> &str {
        self.token
    }

    pub fn get_info(&self) -> SessionInfo {
        SessionInfo {
            now: SystemTime::now(),
            timeout_time: self.bk.get_timeout_time(),
        }
    }

    pub fn list_created_wallets(&self) -> Vec<String> {
        self.bk.list_created_wallets()
    }

    pub fn has_wallet(&self, name: &str) -> Result<bool, BeekeeperError> {
        Ok(self.bk.holder.has_wallet(name)?)
    }

    pub fn create_wallet(
        &mut self,
        name: &str,
        password: Option<&str>,
        is_temporary: Option<bool>,
    ) -> Result<String, BeekeeperError> {
        let is_temp = is_temporary.unwrap_or(self.bk.is_in_memory);
        Ok(self.bk.holder.pin_mut().create_wallet(
            self.token,
            name,
            password.unwrap_or(""),
            is_temp,
        )?)
    }

    pub fn open_wallet(&mut self, name: &str) -> Result<(), BeekeeperError> {
        self.bk.holder.pin_mut().open_wallet(self.token, name)?;
        Ok(())
    }

    pub fn close_wallet(&mut self, name: &str) -> Result<(), BeekeeperError> {
        self.bk.holder.pin_mut().close_wallet(name)?;
        Ok(())
    }
}
