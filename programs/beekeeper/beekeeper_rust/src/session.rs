use crate::api::Beekeeper;

pub struct Session<'a> {
    pub(super) bk: &'a mut Beekeeper,
    pub(super) token: &'a str,
}

impl<'a> Session<'a> {
    pub fn token(&self) -> &str {
        self.token
    }

    pub fn has_wallet(&self, name: &str) -> Result<bool, cxx::Exception> {
        self.bk.holder.has_wallet(name)
    }

    pub fn create_wallet(
        &mut self,
        name: &str,
        password: Option<&str>,
        is_temporary: Option<bool>,
    ) -> Result<String, cxx::Exception> {
        let is_temp = is_temporary.unwrap_or(self.bk.is_in_memory);
        self.bk.holder.pin_mut().create_wallet(
            self.token,
            name,
            password.unwrap_or(""),
            is_temp,
        )
    }

    pub fn open_wallet(&mut self, name: &str) -> Result<(), cxx::Exception> {
        self.bk.holder.pin_mut().open_wallet(self.token, name)
    }

    pub fn close_wallet(&mut self, name: &str) -> Result<(), cxx::Exception> {
        self.bk.holder.pin_mut().close_wallet(name)
    }
}
