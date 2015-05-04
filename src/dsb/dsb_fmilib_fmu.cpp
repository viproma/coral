#include "dsb/fmilib/fmu.hpp"


dsb::fmilib::Fmu::Fmu(std::shared_ptr<dsb::fmilib::ImportContext> context)
    : m_context(context)
{
}


std::shared_ptr<dsb::fmilib::ImportContext> dsb::fmilib::Fmu::Context() const
{
    return m_context;
}
