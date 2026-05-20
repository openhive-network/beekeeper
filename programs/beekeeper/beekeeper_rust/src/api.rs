use std::{
    collections::HashSet,
    fs,
    path::{Path, PathBuf},
    time::{Duration, Instant, SystemTime},
};

use cxx::UniquePtr;

use crate::{
    RustCryptoProtocol,
    consts::{LEGACY_WALLET_DIR, LEGACY_WALLET_EXT},
    errors::BeekeeperError,
    ffi, new_rust_storage_protocol,
    options::BeekeeperOptions,
};

pub struct BeekeeperApi {
    pub(super) holder: UniquePtr<ffi::BeekeeperHolder>,
    sessions: HashSet<String>,
    pub(super) is_in_memory: bool,
    unlock_timeout_ms: u32,
    last_activity: Instant,
    wallet_dir: PathBuf,
}

impl BeekeeperApi {
    pub fn new(options: BeekeeperOptions) -> Self {
        let wallet_dir =
            Path::new(&options.storage_root).join(LEGACY_WALLET_DIR);
        let storage = new_rust_storage_protocol(&options.storage_root);
        let crypto = Box::new(RustCryptoProtocol);
        let holder =
            ffi::new_beekeeper_holder(crypto, storage, options.unlock_timeout);

        Self {
            holder,
            sessions: HashSet::new(),
            is_in_memory: options.in_memory,
            unlock_timeout_ms: options.unlock_timeout.saturating_mul(1000),
            last_activity: Instant::now(),
            wallet_dir,
        }
    }

    pub fn refresh_timeout(&mut self) {
        self.last_activity = Instant::now();
    }

    pub fn throw_if_timed_out_and_refresh(
        &mut self,
    ) -> Result<(), BeekeeperError> {
        if self.is_timed_out() {
            return Err(BeekeeperError::TimedOut);
        }
        self.refresh_timeout();

        Ok(())
    }

    pub fn is_timed_out(&self) -> bool {
        self.unlock_timeout_ms > 0
            && self.last_activity.elapsed().as_millis() as u64
                >= u64::from(self.unlock_timeout_ms)
    }

    pub fn get_timeout_time(&self) -> SystemTime {
        if self.unlock_timeout_ms == 0 {
            return SystemTime::UNIX_EPOCH
                + Duration::from_secs(60 * 60 * 24 * 365 * 100);
        }
        let elapsed = self.last_activity.elapsed();
        let remaining =
            Duration::from_millis(u64::from(self.unlock_timeout_ms))
                .saturating_sub(elapsed);

        SystemTime::now() + remaining
    }

    pub fn list_created_wallets(&self) -> Vec<String> {
        let Ok(entries) = fs::read_dir(&self.wallet_dir) else {
            return Vec::new();
        };
        entries
            .filter_map(|e| e.ok())
            .filter_map(|e| {
                e.file_name()
                    .to_str()?
                    .strip_suffix(LEGACY_WALLET_EXT)
                    .map(String::from)
            })
            .collect()
    }

    pub fn version(&self) -> &'static str {
        env!("CARGO_PKG_VERSION")
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

    pub fn delete(&mut self) -> Result<(), BeekeeperError> {
        let tokens: Vec<String> = self.sessions.drain().collect();
        for token in tokens {
            self.holder.pin_mut().close_session(&token)?;
        }

        Ok(())
    }
}
