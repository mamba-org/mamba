#ifndef MAMBA_ERROR_HANDLING_HPP
#define MAMBA_ERROR_HANDLING_HPP

#include <exception>
#include <vector>

#include "tl/expected.hpp"

namespace mamba
{

    /*********************
     * Mamba esxceptions *
     *********************/

    enum class mamba_error_code
    {
        unknown,
        prefix_data_not_loaded,
        subdirdata_not_loaded,
        cache_not_loaded,
        repodata_not_loaded,
        aggregated
    };

    class mamba_error : public std::runtime_error
    {
    public:
        using base_type = std::runtime_error;

        mamba_error(const std::string& msg, mamba_error_code ec);
        mamba_error(const char* msg, mamba_error_code ec);

        mamba_error_code error_code() const noexcept;

    private:
        mamba_error_code m_error_code;
    };

    class mamba_aggregated_error : public mamba_error
    {
    public:
        using base_type = mamba_error;
        using error_list_t = std::vector<mamba_error>;

        explicit mamba_aggregated_error(error_list_t&& error_list);

        const char* what() const noexcept override;

    private:
        error_list_t m_error_list;
        mutable std::string m_aggregated_message;
        static std::string m_base_message;
    };

    /********************************
     * wrappers around tl::expected *
     ********************************/

    template <class T, class E = mamba_error>
    using expected_t = tl::expected<T, E>;

    tl::unexpected<mamba_error>
    make_unexpected(const char* msg, mamba_error_code ec);
    
    tl::unexpected<mamba_error>
    make_unexpected(const std::string& msg, mamba_error_code sc);

    tl::unexpected<mamba_aggregated_error>
    make_unexpected(std::vector<mamba_error>&& error_list);

    template <class T>
    auto forward_error(const expected_t<T>& exp)
    {
        return tl::make_unexpected(exp.error());
    }
}

#endif
