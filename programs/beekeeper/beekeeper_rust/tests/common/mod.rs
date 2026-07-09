//! Shared test fixtures. Keys + sign data lifted from
//! `__tests__/detailed/beekeeper_api.ts` and `factory.ts`.

#![allow(dead_code)]

use beekeeper::{api::BeekeeperApi, options::BeekeeperOptions};
use tempfile::TempDir;

pub struct Bk {
    pub api: BeekeeperApi,
    /// Kept alive so the storage dir survives for the test duration.
    pub _tmp: Option<TempDir>,
}

/// Persistent (file-backed) BeekeeperApi in a fresh tempdir.
pub fn new_persistent() -> Bk {
    let tmp = TempDir::new().expect("tempdir");
    let api = BeekeeperApi::new(BeekeeperOptions::new(
        tmp.path().to_str().expect("utf8"),
    ));
    Bk {
        api,
        _tmp: Some(tmp),
    }
}

/// In-memory BeekeeperApi.
pub fn new_in_memory() -> Bk {
    let api =
        BeekeeperApi::new(BeekeeperOptions::new("ignored").in_memory(true));
    Bk { api, _tmp: None }
}

/// (wif, pubkey-string) pairs matching `beekeeper_api.ts:8`.
pub const KEYS: &[(&str, &str)] = &[
    (
        "5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT",
        "STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh",
    ),
    (
        "5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78",
        "STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa",
    ),
    (
        "5KNbAE7pLwsLbPUkz6kboVpTR24CycqSNHDG95Y8nbQqSqd6tgS",
        "STM7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH",
    ),
    (
        "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n",
        "STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",
    ),
    (
        "5J8C7BMfvMFXFkvPhHNk2NHGk4zy3jF4Mrpf5k5EzAecuuzqDnn",
        "STM6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm",
    ),
    (
        "5J15npVK6qABGsbdsLnJdaF5esrEWxeejeE3KUx6r534ug4tyze",
        "STM6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW",
    ),
    (
        "5K1gv5rEtHiACVTFq9ikhEijezMh4rkbbTPqu4CAGMnXcTLC1su",
        "STM8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME",
    ),
    (
        "5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw",
        "STM8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4",
    ),
];

/// First sign-data fixture from `beekeeper_api.ts:25`.
pub const SIG_DIGEST_0: &str =
    "390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542";
pub const PUBKEY_KEY3: &str =
    "STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4";
pub const EXPECTED_SIG_0: &str = "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21";
