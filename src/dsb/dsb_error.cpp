#include "dsb/error.hpp"

#include <cassert>
#include <cstring>


namespace dsb
{
namespace error
{

std::string ErrnoMessage(const std::string& msg, int errnoValue) DSB_NOEXCEPT
{
    if (errnoValue == 0) return msg;
    else if (msg.empty()) return std::strerror(errnoValue);
    else return msg + " (" + std::strerror(errnoValue) + ')';
}


namespace
{
    class generic_category_impl : public std::error_category
    {
    public:
        const char* name() const DSB_NOEXCEPT final override { return "dsb"; }

        std::string message(int ev) const final override
        {
            switch (static_cast<generic_error>(ev)) {
                case generic_error::aborted:
                    return "Operation aborted";
                case generic_error::canceled:
                    return "Operation canceled";
                case generic_error::operation_failed:
                    return "Operation failed";
                default:
                    assert(!"Unknown simulation error code");
                    return "Unknown simulation error";
            }
        }
    };

    class sim_category_impl : public std::error_category
    {
    public:
        const char* name() const DSB_NOEXCEPT final override { return "simulation"; }

        std::string message(int ev) const final override
        {
            switch (static_cast<sim_error>(ev)) {
                case sim_error::cannot_perform_timestep:
                    return "Slave unable to perform time step";
                default:
                    assert(!"Unknown simulation error code");
                    return "Unknown simulation error";
            }
        }
    };
}


const std::error_category& generic_category() DSB_NOEXCEPT
{
    static generic_category_impl instance;
    return instance;
}


const std::error_category& sim_category() DSB_NOEXCEPT
{
    static sim_category_impl instance;
    return instance;
}


std::error_code make_error_code(generic_error e) DSB_NOEXCEPT
{
    return std::error_code(
        static_cast<int>(e),
        generic_category());
}


std::error_code make_error_code(sim_error e) DSB_NOEXCEPT
{
    return std::error_code(
        static_cast<int>(e),
        sim_category());
}


std::error_condition make_error_condition(generic_error e) DSB_NOEXCEPT
{
    return std::error_condition(
        static_cast<int>(e),
        generic_category());
}


std::error_condition make_error_condition(sim_error e) DSB_NOEXCEPT
{
    return std::error_condition(
        static_cast<int>(e),
        sim_category());
}


}} // namespace
