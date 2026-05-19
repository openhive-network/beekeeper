use thiserror::Error;

pub type Res<T> = Result<T, BeekeeperError>;

#[derive(Debug, Error)]
pub enum BeekeeperError {
    #[error("Wallet not found: {name}")]
    WalletNotFound { name: String },
    #[error("Failed to write wallet: {name}")]
    WalletWriteFailed { name: String },
}
