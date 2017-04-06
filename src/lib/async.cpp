/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/async.hpp>


namespace coral
{
namespace async
{


// =============================================================================
// CommThreadDead
// =============================================================================

// TODO: Reimplement this stuff in terms of std::nested_exception when that
//       becomes available in Visual Studio.

CommThreadDead::CommThreadDead(std::exception_ptr originalException) CORAL_NOEXCEPT
    : m_originalException{originalException}
{
    assert(m_originalException);
}


std::exception_ptr CommThreadDead::OriginalException() const CORAL_NOEXCEPT
{
    return m_originalException;
}


const char* CommThreadDead::what() const CORAL_NOEXCEPT
{
    return "Background communication thread terminated due to an exception";
}


}}
