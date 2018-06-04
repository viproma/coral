/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/util/filesystem.hpp>

#include <utility>
#include <boost/filesystem.hpp>


namespace coral
{
namespace util
{


coral::util::TempDir::TempDir(const boost::filesystem::path& parent)
{
    if (parent.empty()) {
        m_path = boost::filesystem::temp_directory_path()
            / boost::filesystem::unique_path();
    } else if (parent.is_absolute()) {
        m_path = parent / boost::filesystem::unique_path();
    } else {
        m_path = boost::filesystem::temp_directory_path()
            / parent / boost::filesystem::unique_path();
    }
    boost::filesystem::create_directories(m_path);
}

coral::util::TempDir::TempDir(TempDir&& other) noexcept
    : m_path{std::move(other.m_path)}
{
    // This doesn't seem to be guaranteed by path's move constructor:
    other.m_path.clear();
}

coral::util::TempDir& coral::util::TempDir::operator=(TempDir&& other) noexcept
{
    DeleteNoexcept();
    m_path = std::move(other.m_path);
    // This doesn't seem to be guaranteed by path's move constructor:
    other.m_path.clear();
    return *this;
}

coral::util::TempDir::~TempDir()
{
    DeleteNoexcept();
}

const boost::filesystem::path& coral::util::TempDir::Path() const
{
    return m_path;
}

void coral::util::TempDir::DeleteNoexcept() noexcept
{
    if (!m_path.empty()) {
        boost::system::error_code ignoreErrors;
        boost::filesystem::remove_all(m_path, ignoreErrors);
        m_path.clear();
    }
}


}} // namespace
