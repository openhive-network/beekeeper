use aes::Aes256;
use cbc::cipher::{
    block_padding::Pkcs7, BlockDecryptMut, BlockEncryptMut, KeyIvInit,
};
use k256::{
    ecdsa::SigningKey,
    elliptic_curve::{ecdh::diffie_hellman, sec1::ToEncodedPoint},
    PublicKey, SecretKey,
};
use rand::RngCore;
use ripemd::Ripemd160;
use sha2::{Sha256, Sha512};

use crate::{errors::BeekeeperError, RustCryptoProtocol};

type Aes256CbcEnc = cbc::Encryptor<Aes256>;
type Aes256CbcDec = cbc::Decryptor<Aes256>;

fn crypto_err(e: impl std::fmt::Display) -> BeekeeperError {
    BeekeeperError::Crypto(e.to_string())
}

impl RustCryptoProtocol {
    pub(crate) fn cpp_sha256(&mut self, data: &[u8], out: &mut [u8]) {
        use sha2::Digest;
        out.copy_from_slice(&Sha256::digest(data));
    }

    pub(crate) fn cpp_sha512(&mut self, data: &[u8], out: &mut [u8]) {
        use sha2::Digest;
        out.copy_from_slice(&Sha512::digest(data));
    }

    pub(crate) fn cpp_ripemd160(&mut self, data: &[u8], out: &mut [u8]) {
        use ripemd::Digest;
        out.copy_from_slice(&Ripemd160::digest(data));
    }

    pub(crate) fn cpp_aes256_cbc_encrypt(
        &mut self,
        key: &[u8],
        iv: &[u8],
        data: &[u8],
    ) -> Result<Vec<u8>, BeekeeperError> {
        let cipher = Aes256CbcEnc::new_from_slices(key, iv).map_err(crypto_err)?;
        Ok(cipher.encrypt_padded_vec_mut::<Pkcs7>(data))
    }

    pub(crate) fn cpp_aes256_cbc_decrypt(
        &mut self,
        key: &[u8],
        iv: &[u8],
        data: &[u8],
    ) -> Result<Vec<u8>, BeekeeperError> {
        let cipher = Aes256CbcDec::new_from_slices(key, iv).map_err(crypto_err)?;
        cipher
            .decrypt_padded_vec_mut::<Pkcs7>(data)
            .map_err(crypto_err)
    }

    pub(crate) fn cpp_generate_private_key(&mut self, out: &mut [u8]) {
        let sk = SecretKey::random(&mut rand::thread_rng());
        out.copy_from_slice(&sk.to_bytes());
    }

    pub(crate) fn cpp_get_public_key(
        &mut self,
        privkey: &[u8],
        out: &mut [u8],
    ) -> Result<(), BeekeeperError> {
        let sk = SecretKey::from_slice(privkey).map_err(crypto_err)?;
        let encoded = sk.public_key().to_encoded_point(true);
        out.copy_from_slice(encoded.as_bytes());
        Ok(())
    }

    pub(crate) fn cpp_sign_compact(
        &mut self,
        privkey: &[u8],
        digest: &[u8],
        out: &mut [u8],
    ) -> Result<(), BeekeeperError> {
        let sk = SigningKey::from_slice(privkey).map_err(crypto_err)?;
        let (sig, recid) =
            sk.sign_prehash_recoverable(digest).map_err(crypto_err)?;
        // 65 bytes: [recovery_id + 27 + 4, R(32), S(32)] — matches fc's compact_signature
        out[0] = recid.to_byte() + 27 + 4;
        out[1..65].copy_from_slice(&sig.to_bytes());
        Ok(())
    }

    pub(crate) fn cpp_ecdh_shared_secret(
        &mut self,
        privkey: &[u8],
        pubkey: &[u8],
        out: &mut [u8],
    ) -> Result<(), BeekeeperError> {
        use sha2::Digest;
        let sk = SecretKey::from_slice(privkey).map_err(crypto_err)?;
        let pk = PublicKey::from_sec1_bytes(pubkey).map_err(crypto_err)?;
        let shared = diffie_hellman(sk.to_nonzero_scalar(), pk.as_affine());
        out.copy_from_slice(&Sha512::digest(shared.raw_secret_bytes()));
        Ok(())
    }

    pub(crate) fn cpp_base58_encode(&mut self, data: &[u8]) -> String {
        bs58::encode(data).into_string()
    }

    pub(crate) fn cpp_base58_decode(
        &mut self,
        s: &str,
    ) -> Result<Vec<u8>, BeekeeperError> {
        bs58::decode(s).into_vec().map_err(crypto_err)
    }

    pub(crate) fn cpp_get_random_bytes(&mut self, out: &mut [u8]) {
        rand::thread_rng().fill_bytes(out);
    }
}
