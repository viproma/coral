/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/error.hpp>

#include <cassert>
#include <cstring>


namespace coral
{
namespace error
{

std::string ErrnoMessage(const std::string& msg, int errnoValue) CORAL_NOEXCEPT
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
        const char* name() const CORAL_NOEXCEPT final override { return "generic"; }

        std::string message(int ev) const final override
        {
            switch (static_cast<generic_error>(ev)) {
                case generic_error::aborted:
                    return "Operation aborted";
                case generic_error::canceled:
                    return "Operation canceled";
                case generic_error::operation_failed:
                    return "Operation failed";
                case generic_error::fatal:
                    return "An irrecoverable error occurred";
                default:
                    assert(!"Unknown simulation error code");
                    return "Unknown simulation error";
            }
        }
    };

    class sim_category_impl : public std::error_category
    {
    public:
        const char* name() const CORAL_NOEXCEPT final override { return "simulation"; }

        std::string message(int ev) const final override
        {
            switch (static_cast<sim_error>(ev)) {
                case sim_error::cannot_perform_timestep:
                    return "Slave unable to perform time step";
                case sim_error::data_timeout:
                    return "Slave-to-slave data communication timed out";
                default:
                    assert(!"Unknown simulation error code");
                    return "Unknown simulation error";
            }
        }
    };
}


const std::error_category& generic_category() CORAL_NOEXCEPT
{
    static generic_category_impl instance;
    return instance;
}


const std::error_category& sim_category() CORAL_NOEXCEPT
{
    static sim_category_impl instance;
    return instance;
}


std::error_code make_error_code(generic_error e) CORAL_NOEXCEPT
{
    return std::error_code(
        static_cast<int>(e),
        generic_category());
}


std::error_code make_error_code(sim_error e) CORAL_NOEXCEPT
{
    return std::error_code(
        static_cast<int>(e),
        sim_category());
}


std::error_condition make_error_condition(generic_error e) CORAL_NOEXCEPT
{
    return std::error_condition(
        static_cast<int>(e),
        generic_category());
}


std::error_condition make_error_condition(sim_error e) CORAL_NOEXCEPT
{
    return std::error_condition(
        static_cast<int>(e),
        sim_category());
}


}} // namespace
