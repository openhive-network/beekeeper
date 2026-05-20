use std::{
    collections::{HashMap, HashSet},
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

#[derive(Clone, Copy)]
pub(crate) struct WalletMeta {
    pub is_temporary: bool,
}

pub struct BeekeeperApi {
    pub(super) holder: UniquePtr<ffi::BeekeeperHolder>,
    sessions: HashSet<String>,
    pub(crate) wallets_meta: HashMap<(String, String), WalletMeta>,
    pub(super) is_in_memory: bool,
    unlock_timeout_ms: u32,
    last_activity: Instant,
    wallet_dir: PathBuf,
}

impl BeekeeperApi {
    pub fn new(options: BeekeeperOptions) -> Self {
        let crypto = Box::new(RustCryptoProtocol);
        let (holder, wallet_dir) = if options.in_memory {
            (
                ffi::new_beekeeper_holder_in_memory(
                    crypto,
                    options.unlock_timeout,
                ),
                PathBuf::new(),
            )
        } else {
            let wallet_dir =
                Path::new(&options.storage_root).join(LEGACY_WALLET_DIR);
            let storage = new_rust_storage_protocol(&options.storage_root);
            let holder = ffi::new_beekeeper_holder(
                crypto,
                storage,
                options.unlock_timeout,
            );
            (holder, wallet_dir)
        };

        Self {
            holder,
            sessions: HashSet::new(),
            wallets_meta: HashMap::new(),
            is_in_memory: options.in_memory,
            unlock_timeout_ms: options.unlock_timeout.saturating_mul(1000),
            last_activity: Instant::now(),
            wallet_dir,
        }
    }

    pub(crate) fn register_wallet(
        &mut self,
        token: &str,
        name: &str,
        is_temporary: bool,
    ) {
        self.wallets_meta.insert(
            (token.to_string(), name.to_string()),
            WalletMeta { is_temporary },
        );
    }

    pub(crate) fn unregister_wallet(&mut self, token: &str, name: &str) {
        self.wallets_meta
            .remove(&(token.to_string(), name.to_string()));
    }

    pub(crate) fn wallet_is_temporary(&self, token: &str, name: &str) -> bool {
        self.wallets_meta
            .get(&(token.to_string(), name.to_string()))
            .map(|meta| meta.is_temporary)
            .unwrap_or(false)
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
        if self.is_in_memory {
            return Vec::new();
        }
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
        self.wallets_meta.retain(|(t, _), _| t != token);

        Ok(())
    }

    pub fn delete(&mut self) -> Result<(), BeekeeperError> {
        let tokens: Vec<String> = self.sessions.drain().collect();
        for token in tokens {
            self.holder.pin_mut().close_session(&token)?;
        }
        self.wallets_meta.clear();
        self.holder.pin_mut().sync_storage();
        self.holder.pin_mut().close_storage();

        Ok(())
    }
}
