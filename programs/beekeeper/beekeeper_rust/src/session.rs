use crate::{api::Beekeeper, errors::BeekeeperError};

pub struct Session<'a> {
    pub(super) bk: &'a mut Beekeeper,
    pub(super) token: &'a str,
}

impl<'a> Session<'a> {
    pub fn token(&self) -> &str {
        self.token
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
