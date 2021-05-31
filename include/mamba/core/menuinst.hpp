#include "mamba_fs.hpp"
#include "transaction_context.hpp"

namespace mamba
{
    void remove_menu_from_json(const fs::path& json_file, TransactionContext* context);
    void create_menu_from_json(const fs::path& json_file, TransactionContext* context);
}
