# Copyright (C) 2019, QuantStack
# SPDX-License-Identifier: BSD-3-Clause

from conda.base.context import context
from conda.cli import common as cli_common
from conda.exceptions import (
    CondaExitZero,
    CondaSystemExit,
    DryRunExit,
    PackagesNotFoundError,
)

from libmambapy import cancel_json_output as cancel_mamba_json_output


def handle_txn(unlink_link_transaction, prefix, args, newenv, remove_op=False):
    if context.json:
        cancel_mamba_json_output()

    if unlink_link_transaction.nothing_to_do:
        if remove_op:
            # No packages found to remove from environment
            raise PackagesNotFoundError(args.package_names)
        elif not newenv:
            if context.json:
                cli_common.stdout_json_success(
                    message="All requested packages already installed."
                )
            return

    if context.dry_run:
        actions = unlink_link_transaction._make_legacy_action_groups()[0]
        if context.json:
            cli_common.stdout_json_success(prefix=prefix, actions=actions, dry_run=True)
        raise DryRunExit()

    try:
        unlink_link_transaction.download_and_extract()
        if context.download_only:
            raise CondaExitZero(
                "Package caches prepared. UnlinkLinkTransaction cancelled with "
                "--download-only option."
            )
        unlink_link_transaction.execute()

    except SystemExit as e:
        raise CondaSystemExit("Exiting", e)

    if context.json:
        actions = unlink_link_transaction._make_legacy_action_groups()[0]
        cli_common.stdout_json_success(prefix=prefix, actions=actions)
