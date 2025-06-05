// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

namespace mamba
{
    class TransactionContext;

    namespace fs
    {
        class u8path;
    }

    void
    remove_menu_from_json(const fs::u8path& json_file, const TransactionContext& transaction_context);
    void
    create_menu_from_json(const fs::u8path& json_file, const TransactionContext& transaction_context);
}
