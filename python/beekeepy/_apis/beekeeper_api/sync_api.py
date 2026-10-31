from __future__ import annotations

from hiveio_api.client import AbstractSyncApi, ApiArgumentsToSerialize, SyncSendable

import beekeepy._generated.beekeeper_api_description as beekeeper_api  # noqa: TC001  # resolved at runtime by get_type_hints
from beekeepy._apis.abc.session_holder import SyncSessionHolder
from beekeepy._apis.apply_session_token import sync_apply_session_token
from beekeepy._apis.beekeeper_api.beekeeper_api_commons import BeekeeperApiCommons


class BeekeeperApi(AbstractSyncApi, BeekeeperApiCommons[SyncSendable]):
    api = AbstractSyncApi.endpoint_jsonrpc

    _owner: SyncSessionHolder

    def __init__(self, owner: SyncSessionHolder) -> None:
        self._verify_is_owner_can_hold_session_token(owner=owner)
        super().__init__(owner=owner)

    def _additional_arguments_actions(
        self, endpoint_name: str, arguments: ApiArgumentsToSerialize
    ) -> ApiArgumentsToSerialize:
        if not self._token_required(endpoint_name):
            return super()._additional_arguments_actions(endpoint_name, arguments)
        return sync_apply_session_token(self._owner, arguments)

    def _get_requires_session_holder_type(self) -> type[SyncSessionHolder]:
        return SyncSessionHolder

    @api
    def create(
        self, *, wallet_name: str, password: str | None = None, is_temporary: bool = False, token: str | None = None
    ) -> beekeeper_api.CreateResponse:
        """
        Creates wallet with given name.

        Args:
            wallet_name: name of wallet to create
            password: password for new wallet, if not given generates one. Defaults to None.
            is_temporary: if set, the wallet exists only in memory of beekeeper (it is not saved into a file).
            token: token of the session, the session of the owner of the API when not given.

        Returns:
            beekeeper_api.CreateResponse: Returns password that was used for wallet creation.
        """
        raise NotImplementedError

    @api
    def open(self, *, wallet_name: str, token: str | None = None) -> beekeeper_api.OpenResponse:
        raise NotImplementedError

    @api
    def set_timeout(self, *, seconds: int, token: str | None = None) -> beekeeper_api.SetTimeoutResponse:
        raise NotImplementedError

    @api
    def lock_all(self, token: str | None = None) -> beekeeper_api.LockAllResponse:
        raise NotImplementedError

    @api
    def lock(self, *, wallet_name: str, token: str | None = None) -> beekeeper_api.LockResponse:
        raise NotImplementedError

    @api
    def unlock(self, *, wallet_name: str, password: str, token: str | None = None) -> beekeeper_api.UnlockResponse:
        raise NotImplementedError

    @api
    def import_key(
        self, *, wallet_name: str, wif_key: str, token: str | None = None
    ) -> beekeeper_api.ImportKeyResponse:
        raise NotImplementedError

    @api
    def import_keys(
        self, *, wallet_name: str, wif_keys: list[str], token: str | None = None
    ) -> beekeeper_api.ImportKeysResponse:
        raise NotImplementedError

    @api
    def remove_key(
        self, *, wallet_name: str, public_key: str, token: str | None = None
    ) -> beekeeper_api.RemoveKeyResponse:
        raise NotImplementedError

    @api
    def list_wallets(self, token: str | None = None) -> beekeeper_api.ListWalletsResponse:
        raise NotImplementedError

    @api
    def list_created_wallets(self, token: str | None = None) -> beekeeper_api.ListCreatedWalletsResponse:
        raise NotImplementedError

    @api
    def get_public_keys(
        self, wallet_name: str | None = None, token: str | None = None
    ) -> beekeeper_api.GetPublicKeysResponse:
        raise NotImplementedError

    @api
    def sign_digest(
        self, *, sig_digest: str, public_key: str, wallet_name: str | None = None, token: str | None = None
    ) -> beekeeper_api.SignDigestResponse:
        raise NotImplementedError

    @api
    def get_info(self, token: str | None = None) -> beekeeper_api.GetInfoResponse:
        raise NotImplementedError

    @api
    def create_session(self, *, salt: str = "") -> beekeeper_api.CreateSessionResponse:
        raise NotImplementedError

    @api
    def close_session(self, token: str | None = None) -> beekeeper_api.CloseSessionResponse:
        raise NotImplementedError

    @api
    def close(self, wallet_name: str, token: str | None = None) -> beekeeper_api.CloseResponse:
        raise NotImplementedError

    @api
    def has_matching_private_key(
        self, wallet_name: str, public_key: str, token: str | None = None
    ) -> beekeeper_api.HasMatchingPrivateKeyResponse:
        raise NotImplementedError

    @api
    def encrypt_data(
        self,
        *,
        wallet_name: str,
        from_public_key: str,
        to_public_key: str,
        content: str,
        nonce: int | None = None,
        token: str | None = None,
    ) -> beekeeper_api.EncryptDataResponse:
        raise NotImplementedError

    @api
    def decrypt_data(
        self,
        *,
        wallet_name: str,
        from_public_key: str,
        to_public_key: str,
        encrypted_content: str,
        token: str | None = None,
    ) -> beekeeper_api.DecryptDataResponse:
        raise NotImplementedError

    @api
    def get_version(self) -> beekeeper_api.GetVersionResponse:
        """Gets version of beekeeper - git revision it was built from (no session is required).

        Returns:
            Version of beekeeper.
        """
        raise NotImplementedError

    @api
    def has_wallet(self, *, wallet_name: str, token: str | None = None) -> beekeeper_api.HasWalletResponse:
        """Checks if wallet with given name exists (is opened or stored in directory of wallets).

        Args:
            wallet_name: wallet to look for
            token: token of the session, the session of the owner of the API when not given.

        Returns:
            True if the wallet exists, False otherwise
        """
        raise NotImplementedError

    @api
    def is_wallet_unlocked(
        self, *, wallet_name: str, token: str | None = None
    ) -> beekeeper_api.IsWalletUnlockedResponse:
        """Checks if given wallet is unlocked in current session.

        Args:
            wallet_name: wallet to check
            token: token of the session, the session of the owner of the API when not given.

        Returns:
            True if the wallet is unlocked, False otherwise (also when it is not opened in current session)
        """
        raise NotImplementedError
