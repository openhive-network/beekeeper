//! Mirrors `beekeeper_wasm/__tests__/detailed/base.ts`.
//!
//! Only the first TS test has a real Rust analog. The second TS test
//! ("Should be able to create instance of BeekeeperInstanceHelper") exercises
//! a test-only fixture defined in `__tests__/assets/run_node_helper.js` —
//! there is no public API behind it, so it's not portable.
//!
//! We add a second test for persistent-backend construction since Rust covers
//! both paths through one type and we want them both smoke-tested.

use beekeeper_rust::{api::BeekeeperApi, options::BeekeeperOptions};
use tempfile::TempDir;

/// Port of "Should be able to create instance of beekeeper_api".
/// TS constructs `beekeeper_api` with a Map-backed in-memory storage callback
/// and the crypto callbacks; the Rust equivalent is `in_memory(true)`.
#[test]
fn can_create_beekeeper_api_in_memory() {
    let _bk = BeekeeperApi::new(
        BeekeeperOptions::new("ignored-in-memory-mode").in_memory(true),
    );
}

/// Rust-only addition: smoke-test the persistent backend path.
/// TS doesn't have an equivalent at the `base.ts` level — `WALLET_OPTIONS_NODE`
/// flows through `BeekeeperInstanceHelper` which is a test fixture, not a
/// public API surface.
#[test]
fn can_create_beekeeper_api_persistent() {
    let tmp = TempDir::new().expect("tempdir");
    let _bk = BeekeeperApi::new(BeekeeperOptions::new(
        tmp.path().to_str().expect("utf8 path"),
    ));
}
