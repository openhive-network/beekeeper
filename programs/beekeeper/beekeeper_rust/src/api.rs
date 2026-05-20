use std::{collections::HashSet, time::Instant};

use cxx::UniquePtr;

use crate::{RustCryptoProtocol, ffi, new_rust_storage_protocol};

pub struct Beekeeper {
    holder: UniquePtr<ffi::beekeeper_holder>,
    sessions: HashSet<String>,
    is_in_memory: bool,
    unlock_timeout_ms: u32,
    last_activity: Instant,
}

impl Beekeeper {
    pub fn new(
        storage_root: &str,
        unlock_timeout_sec: u32,
        is_in_memory: bool,
    ) -> Self {
        let storage = new_rust_storage_protocol(storage_root);
        let crypto = Box::new(RustCryptoProtocol);
        let holder =
            ffi::new_beekeeper_holder(crypto, storage, unlock_timeout_sec);
        Self {
            holder,
            sessions: HashSet::new(),
            is_in_memory,
            unlock_timeout_ms: unlock_timeout_sec.saturating_mul(1000),
            last_activity: Instant::now(),
        }
    }

    pub fn is_in_memory(&self) -> bool {
        self.is_in_memory
    }

    pub fn is_timed_out(&self) -> bool {
        self.unlock_timeout_ms > 0
            && self.last_activity.elapsed().as_millis() as u64
                >= u64::from(self.unlock_timeout_ms)
    }

    pub fn refresh_timeout(&mut self) {
        self.last_activity = Instant::now();
    }

    pub fn create_session(&mut self) -> Result<String, cxx::Exception> {
        let token = self.holder.pin_mut().create_session()?;
        self.sessions.insert(token.clone());
        Ok(token)
    }

    pub fn close_session(&mut self, token: &str) -> Result<(), cxx::Exception> {
        self.holder.pin_mut().close_session(token)?;
        self.sessions.remove(token);
        Ok(())
    }

    pub fn session<'a>(&'a mut self, token: &'a str) -> Session<'a> {
        Session { bk: self, token }
    }

    pub fn lock_all(&mut self) -> Result<(), cxx::Exception> {
        self.holder.pin_mut().lock_all()
    }

    pub fn unlock(
        &mut self,
        name: &str,
        password: &str,
    ) -> Result<(), cxx::Exception> {
        self.holder.pin_mut().unlock(name, password)
    }

    pub fn set_timeout(&mut self, seconds: u32) {
        self.holder.pin_mut().set_timeout(seconds);
        self.unlock_timeout_ms = seconds.saturating_mul(1000);
    }
}

/// Short-lived borrow of a Beekeeper scoped to a particular session token.
pub struct Session<'a> {
    bk: &'a mut Beekeeper,
    token: &'a str,
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
