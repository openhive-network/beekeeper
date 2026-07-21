//! Construction options for [`BeekeeperApi`](crate::api::BeekeeperApi).
//!
//! Mirrors the `IBeekeeperOptions` TypeScript interface (see
//! `beekeeper_wasm/src/detailed/interfaces.ts`). The `wasmLocation` field
//! has no Rust analogue (there is no WASM artefact to locate), and the
//! Rust API uses a builder rather than a single options struct literal.

/// Default [`BeekeeperOptions::storage_root`], matching the TS web default.
pub const DEFAULT_STORAGE_ROOT: &str = "/storage_root";

/// Configuration consumed by [`BeekeeperApi::new`](crate::api::BeekeeperApi::new).
///
/// Built with [`BeekeeperOptions::new`] and a chain of setters. All fields
/// are public so that callers can also construct the struct directly when
/// that reads more clearly than the builder.
///
/// # Difference from TS
///
/// - The TS interface (`IBeekeeperOptions`) carries `wasmLocation`; this
///   field is omitted here because the Rust crate links directly to the
///   shared C++ core.
/// - TS defaults `storageRoot` to `"/storage_root"` (web) /
///   `"./storage_root-node"` (Node) at the factory layer. The Rust
///   [`Default`] uses [`DEFAULT_STORAGE_ROOT`] (`"/storage_root"`).
pub struct BeekeeperOptions {
    /// Parent directory of the `.beekeeper` wallet directory used by the
    /// filesystem-backed storage. Ignored when [`Self::in_memory`] is `true`.
    pub storage_root: String,

    /// Inactivity timeout in **seconds** after which unlocked wallets are
    /// auto-locked. A value of `0` disables the timeout. Defaults to `900`
    /// (15 minutes), matching the TS default.
    pub unlock_timeout: u32,

    /// When `true`, no filesystem storage is created and wallets live only
    /// in process memory. Useful for one-shot operations (e.g. import key
    /// + sign + drop). Maps to TS `inMemory` (default `false`).
    pub in_memory: bool,
}

impl Default for BeekeeperOptions {
    /// [`DEFAULT_STORAGE_ROOT`], a 900-second unlock timeout, persistent
    /// mode — the same defaults the TS factory applies when called with no
    /// options.
    fn default() -> Self {
        Self::new(DEFAULT_STORAGE_ROOT)
    }
}

impl BeekeeperOptions {
    /// Start a new options builder pinned to the given wallet storage root.
    ///
    /// The directory is created lazily by
    /// [`new_rust_storage_protocol`](crate::new_rust_storage_protocol) when
    /// the [`BeekeeperApi`](crate::api::BeekeeperApi) is constructed in
    /// persistent mode.
    pub fn new(storage_root: impl Into<String>) -> Self {
        Self {
            storage_root: storage_root.into(),
            unlock_timeout: 900,
            in_memory: false,
        }
    }

    /// Override the inactivity timeout (seconds). Pass `0` to disable.
    pub fn unlock_timeout(mut self, seconds: u32) -> Self {
        self.unlock_timeout = seconds;
        self
    }

    /// Toggle in-memory mode. When `true` the `storage_root` value is
    /// retained for symmetry but is never used.
    pub fn in_memory(mut self, in_memory: bool) -> Self {
        self.in_memory = in_memory;
        self
    }
}
