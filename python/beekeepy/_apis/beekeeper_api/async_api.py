from __future__ import annotations

from hiveio_api.client import AbstractAsyncApi, ApiArgumentsToSerialize, AsyncSendable

import beekeepy._generated.beekeeper_api_description as beekeeper_api  # noqa: TCH001  # resolved at runtime by get_type_hints
from beekeepy._apis.abc.session_holder import AsyncSessionHolder
from beekeepy._apis.apply_session_token import async_apply_session_token
from beekeepy._apis.beekeeper_api.beekeeper_api_commons import BeekeeperApiCommons


class BeekeeperApi(AbstractAsyncApi, BeekeeperApiCommons[AsyncSendable]):
    """Set of endpoints, that allows asynchronous communication with beekeeper service."""

    api = AbstractAsyncApi.endpoint_jsonrpc
    _owner: AsyncSessionHolder

    def __init__(self, owner: AsyncSessionHolder) -> None:
        self._verify_is_owner_can_hold_session_token(owner=owner)
        super().__init__(owner=owner)

    async def _additional_arguments_actions(
        self, endpoint_name: str, arguments: ApiArgumentsToSerialize
    ) -> ApiArgumentsToSerialize:
        if not self._token_required(endpoint_name):
            return await super()._additional_arguments_actions(endpoint_name, arguments)
        return await async_apply_session_token(self._owner, arguments)

    def _get_requires_session_holder_type(self) -> type[AsyncSessionHolder]:
        return AsyncSessionHolder

    @api
    async def create(
        self, *, wallet_name: str, password: str | None = None, is_temporary: bool = False, token: str | None = None
    ) -> beekeeper_api.CreateResponse:
        """
        Creates wallet with given name.

        Args:
            wallet_name: name of wallet to create
            password: password for new wallet, if not given generates one. Defaults to None.
            is_temporary: if set, the wallet exists only in memory of beekeeper (it is not saved into a file).

        Returns
            beekeeper_api.CreateResponse: Returns password that was used for wallet creation.
        """
        raise NotImplementedError

    @api
    async def open(self, *, wallet_name: str, token: str | None = None) -> beekeeper_api.OpenResponse:
        """Opens wallet, which makes it unaccessible for other sessions.

        Args:
            wallet_name: wallet to open

        Returns:
            Nothing.
        """
        raise NotImplementedError

    @api
    async def set_timeout(self, *, seconds: int, token: str | None = None) -> beekeeper_api.SetTimeoutResponse:
        """Sets timeout after all wallets opened in current session will be closed.

        Args:
            seconds: amount of seconds from last interaction (on any wallet) to wait before lock all wallets.

        Returns
            noting.
        """
        raise NotImplementedError

    @api
    async def lock_all(self, token: str | None = None) -> beekeeper_api.LockAllResponse:
        """Locks all wallet in current session.

        Returns:
            Nothing.
        """
        raise NotImplementedError

    @api
    async def lock(self, *, wallet_name: str, token: str | None = None) -> beekeeper_api.LockResponse:
        """Locks specific wallet.

        Args:
            wallet_name: wallet to lock.

        Returns:
            Nothing.
        """
        raise NotImplementedError

    @api
    async def unlock(
        self, *, wallet_name: str, password: str, token: str | None = None
    ) -> beekeeper_api.UnlockResponse:
        """Unlocks specific wallet (and opens it implicitly).

        Args:
            wallet_name: wallet to unlock (and open)
            password: password to unlock wallet

        Returns:
            Nothing.
        """
        raise NotImplementedError

    @api
    async def import_key(
        self, *, wallet_name: str, wif_key: str, token: str | None = None
    ) -> beekeeper_api.ImportKeyResponse:
        """Imports key to given wallet.

        Warning:
            To read private key you need access to wallet file and beekeeper binary

        Args:
            wallet_name: wallet to import given key
            wif_key: private key to import

        Returns:
            Public key calculated from imported private key.
        """
        raise NotImplementedError

    @api
    async def import_keys(
        self, *, wallet_name: str, wif_keys: list[str], token: str | None = None
    ) -> beekeeper_api.ImportKeysResponse:
        """Imports multiple keys to a given wallet at once.

        Warning:
            To read private keys you need access to wallet file and beekeeper binary.

        Args:
            wallet_name: Wallet to import the given key.
            wif_keys: List of private keys to import.

        Returns:
            Public keys calculated from the imported private keys.
        """
        raise NotImplementedError

    @api
    async def remove_key(
        self, *, wallet_name: str, public_key: str, token: str | None = None
    ) -> beekeeper_api.RemoveKeyResponse:
        """Removes imported key from given wallet.

        Args:
            wallet_name: wallet from key will be removed.
            public_key: public key, which is paired with private key to remove.

        Returns:
            Nothing.
        """
        raise NotImplementedError

    @api
    async def list_wallets(self, token: str | None = None) -> beekeeper_api.ListWalletsResponse:
        """Lists all opened wallets in current session.

        Note:
            Use `list_created_wallets` to get all (even not opened) wallets

        Returns:
            List of wallets
        """
        raise NotImplementedError

    @api
    async def list_created_wallets(self, token: str | None = None) -> beekeeper_api.ListCreatedWalletsResponse:
        """Lists all wallets existing in beekeeper.

        Returns:
            List of wallets
        """
        raise NotImplementedError

    @api
    async def get_public_keys(
        self, wallet_name: str | None = None, token: str | None = None
    ) -> beekeeper_api.GetPublicKeysResponse:
        """Lists all public keys from all unlocked wallets.

        Args:
            wallet_name: if not None, beekeeper will list public keys only from given wallet

        Returns:
            List of public keys
        """
        raise NotImplementedError

    @api
    async def sign_digest(
        self, *, sig_digest: str, public_key: str, wallet_name: str | None = None, token: str | None = None
    ) -> beekeeper_api.SignDigestResponse:
        """Signs given digest with private key paired with given public key.

        Args:
            sig_digest: digest to sign
            public_key: public key paired with private key to sign with
            wallet_name: if not set to None, beekeeper will use keys only from given wallet

        Returns:
            Signature
        """
        raise NotImplementedError

    @api
    async def get_info(self, token: str | None = None) -> beekeeper_api.GetInfoResponse:
        """Gets status of current session.

        Returns:
            Information about current session.
        """
        raise NotImplementedError

    @api
    async def create_session(self, *, salt: str = "") -> beekeeper_api.CreateSessionResponse:
        """Creates session.

        Note:
            This is called automatically when connection with beekeeper is establish, no need to call it explicitly.

        Args:
            salt: used for generation of session token

        Returns:
            Session token.
        """
        raise NotImplementedError

    @api
    async def close_session(self, token: str | None = None) -> beekeeper_api.CloseSessionResponse:
        """Closes session.

        Returns:
            Noting.
        """
        raise NotImplementedError

    @api
    async def close(self, wallet_name: str, token: str | None = None) -> beekeeper_api.CloseResponse:
        """Closes opened wallet, which implies locking.

        Args:
            wallet_name: wallet name to close

        Returns:
            Nothing
        """
        raise NotImplementedError

    @api
    async def has_matching_private_key(
        self, wallet_name: str, public_key: str, token: str | None = None
    ) -> beekeeper_api.HasMatchingPrivateKeyResponse:
        """Checks is beekeeper contain private key associated with given public key.

        Args:
            wallet_name: wallet to check for corresponding private key
            public_key: public key to check

        Returns:
            True if found, False otherwise
        """
        raise NotImplementedError

    @api
    async def encrypt_data(
        self,
        *,
        wallet_name: str,
        from_public_key: str,
        to_public_key: str,
        content: str,
        nonce: int | None = None,
        token: str | None = None,
    ) -> beekeeper_api.EncryptDataResponse:
        """Encrypts given buffer.

        Args:
            from_public_key: used to match private key for encryption.
            to_public_key: used to encrypt for receiver.
            content: buffer to encrypt.
            nonce: used as seed, to have deterministic encryption. Defaults to None.

        Returns:
            Encrypted memo
        """
        raise NotImplementedError

    @api
    async def decrypt_data(
        self,
        *,
        wallet_name: str,
        from_public_key: str,
        to_public_key: str,
        encrypted_content: str,
        token: str | None = None,
    ) -> beekeeper_api.DecryptDataResponse:
        """Decrypts given buffer.

        Args:
            from_public_key: used to match private key for decryption.
            to_public_key: used to decrypt from sender.
            encrypted_content: buffer to decrypt.

        Returns:
            Decrypted memo
        """
        raise NotImplementedError

    @api
    async def get_version(self) -> beekeeper_api.GetVersionResponse:
        """Gets version of beekeeper - git revision it was built from (no session is required).

        Returns:
            Version of beekeeper.
        """
        raise NotImplementedError

    @api
    async def has_wallet(self, *, wallet_name: str, token: str | None = None) -> beekeeper_api.HasWalletResponse:
        """Checks if wallet with given name exists (is opened or stored in directory of wallets).

        Args:
            wallet_name: wallet to look for

        Returns:
            True if the wallet exists, False otherwise
        """
        raise NotImplementedError

    @api
    async def is_wallet_unlocked(
        self, *, wallet_name: str, token: str | None = None
    ) -> beekeeper_api.IsWalletUnlockedResponse:
        """Checks if given wallet is unlocked in current session.

        Args:
            wallet_name: wallet to check

        Returns:
            True if the wallet is unlocked, False otherwise (also when it is not opened in current session)
        """
        raise NotImplementedError
