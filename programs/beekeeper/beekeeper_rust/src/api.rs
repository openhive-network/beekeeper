use std::{collections::HashSet, time::Instant};

use cxx::UniquePtr;

use crate::{
    RustCryptoProtocol, errors::BeekeeperError, ffi, new_rust_storage_protocol,
    session::Session,
};

pub struct Beekeeper {
    pub(super) holder: UniquePtr<ffi::BeekeeperHolder>,
    sessions: HashSet<String>,
    pub(super) is_in_memory: bool,
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

    pub fn create_session(&mut self) -> Result<String, BeekeeperError> {
        let token = self.holder.pin_mut().create_session()?;
        self.sessions.insert(token.clone());
        Ok(token)
    }

    pub fn close_session(&mut self, token: &str) -> Result<(), BeekeeperError> {
        self.holder.pin_mut().close_session(token)?;
        self.sessions.remove(token);
        Ok(())
    }

    pub fn session<'a>(&'a mut self, token: &'a str) -> Session<'a> {
        Session { bk: self, token }
    }

    pub fn lock_all(&mut self) -> Result<(), BeekeeperError> {
        self.holder.pin_mut().lock_all()?;
        Ok(())
    }

    pub fn unlock(
        &mut self,
        name: &str,
        password: &str,
    ) -> Result<(), BeekeeperError> {
        self.holder.pin_mut().unlock(name, password)?;
        Ok(())
    }

    pub fn set_timeout(&mut self, seconds: u32) {
        self.holder.pin_mut().set_timeout(seconds);
        self.unlock_timeout_ms = seconds.saturating_mul(1000);
    }
}
