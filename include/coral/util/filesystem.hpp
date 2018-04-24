/**
 *  \file
 *  \brief Filesystem utilities.
 *  \copyright
 *      Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
 *      This Source Code Form is subject to the terms of the Mozilla Public
 *      License, v. 2.0. If a copy of the MPL was not distributed with this
 *      file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef CORAL_UTIL_FILESYSTEM_HPP
#define CORAL_UTIL_FILESYSTEM_HPP

#include <boost/filesystem.hpp>
#include <coral/config.h>


namespace coral
{
namespace util
{


/**
 *  \brief  An RAII object that creates a unique directory on construction
 *          and recursively deletes it again on destruction.
*/
class TempDir
{
public:
    /**
     *  \brief  Creates a new temporary directory.
     *
     *  The name of the new directory will be randomly generated, and there are
     *  three options of where it will be created, depending on the value of
     *  `parent`.  In the following, `temp` refers to a directory suitable for
     *  temporary files under the conventions of the operating system (e.g.
     *  `/tmp` under UNIX-like systems), and `name` refers to the randomly
     *  generated name mentioned above.
     *
     *    - If `parent` is empty: `temp/name`
     *    - If `parent` is relative: `temp/parent/name`
     *    - If `parent` is absolute: `parent/name`
    */
    explicit TempDir(
        const boost::filesystem::path& parent = boost::filesystem::path());

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    /**
     *  \brief  Move constructor.
     *
     *  Ownership of the directory is transferred from `other` to `this`.
     *  Afterwards, `other` no longer refers to any directory, meaning that
     *  `other.Path()` will return an empty path, and its destructor will not
     *  perform any filesystem operations.
    */
    TempDir(TempDir&& other) noexcept;

    /// Move assignment operator. See TempDir(TempDir&&) for semantics.
    TempDir& operator=(TempDir&&) noexcept;

    /// Destructor.  Recursively deletes the directory.
    ~TempDir() noexcept;

    /// Returns the path to the directory.
    const boost::filesystem::path& Path() const;

private:
    void DeleteNoexcept() noexcept;

    boost::filesystem::path m_path;
};


}} // namespace
#endif // header guard
