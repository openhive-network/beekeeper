//! Error type returned by the public Rust API.
//!
//! Counterpart of `class BeekeeperError extends Error` in
//! `beekeeper_wasm/src/detailed/errors.ts`. The TS layer collapses every
//! failure into a single `BeekeeperError` instance with a `message` string;
//! Rust keeps a small enum so callers can pattern-match on the common
//! cases without parsing error text.

use thiserror::Error;

/// Convenient `Result<T, BeekeeperError>` alias used internally.
pub type Res<T> = Result<T, BeekeeperError>;

/// Every failure surfaced by the Rust facade.
///
/// Errors raised by the C++ wallet manager arrive as
/// [`cxx::Exception`] and are wrapped in
/// [`BeekeeperError::Cxx`] verbatim — their `Display` message is the text
/// thrown on the C++ side (typically an `fc::exception` message).
#[derive(Debug, Error)]
pub enum BeekeeperError {
    /// The named wallet file is missing from the storage directory.
    ///
    /// Only emitted by [`RustStorageProtocol::cpp_load`](crate::storage::RustStorageProtocol),
    /// i.e. when the C++ side asks for a wallet that does not exist on disk.
    #[error("Wallet not found: {name}")]
    WalletNotFound { name: String },

    /// Persisting wallet bytes to disk failed (permissions, full disk, …).
    #[error("Failed to write wallet: {name}")]
    WalletWriteFailed { name: String },

    /// A crypto primitive (AES, secp256k1, base58, …) refused the input.
    ///
    /// The wrapped string is the lower-level error's `Display` text.
    #[error("Crypto error: {0}")]
    Crypto(String),

    /// An operation was attempted after the inactivity timeout elapsed.
    ///
    /// Matches the TS behaviour where unlocked-wallet operations call
    /// `throwIfTimedOutAndRefresh()` and throw with this exact message.
    #[error("Wallet locked due to timeout")]
    TimedOut,

    /// Pass-through for exceptions thrown by the C++ wallet manager.
    #[error(transparent)]
    Cxx(#[from] cxx::Exception),
}
