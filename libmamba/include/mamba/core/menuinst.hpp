#include "mamba/fs/filesystem.hpp"

#include "transaction_context.hpp"

namespace mamba
{
    void remove_menu_from_json(
        const Context& context,
        const fs::u8path& json_file,
        TransactionContext* transaction_context
    );
    void create_menu_from_json(
        const Context& context,
        const fs::u8path& json_file,
        TransactionContext* transaction_context
    );
#ifdef _WIN32
    namespace win
    {
        fs::u8path get_folder(const std::string& id);
    }

#endif
}
