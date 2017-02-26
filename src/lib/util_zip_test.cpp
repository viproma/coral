#include <cstdlib>
#include <ios>

#include "boost/filesystem.hpp"
#include "gtest/gtest.h"

#include "coral/util.hpp"
#include "coral/util/zip.hpp"


TEST(coral_util_zip, Archive)
{
    namespace du = coral::util;
    namespace dz = coral::util::zip;
    namespace fs = boost::filesystem;

    // Info about the test archive file and its contents
    const std::uint64_t archiveEntryCount = 3;
    const std::string dirFilename = "images/";
    const std::string binFilename = "smiley.png";
    const std::string txtFilename = "a text file.txt";
    const std::string dirName = dirFilename;
    const std::string binName = dirFilename + binFilename;
    const std::string txtName = txtFilename;
    const std::uint64_t binSize = 16489;
    const std::uint64_t txtSize = 13;

    // Test setup
    const auto testDataDir = std::getenv("CORAL_TEST_DATA_DIR");
    ASSERT_STRNE(nullptr, testDataDir);
    const auto archivePath = boost::filesystem::path(testDataDir) / "ziptest.zip";

    // Open archive
    auto archive = dz::Archive(archivePath);
    ASSERT_TRUE(archive.IsOpen());

    // Get entry info
    ASSERT_EQ(archiveEntryCount, archive.EntryCount());
    const auto dirIndex = archive.FindEntry(dirName);
    const auto binIndex = archive.FindEntry(binName);
    const auto txtIndex = archive.FindEntry(txtName);
    const auto invIndex = archive.FindEntry("no such entry");
    ASSERT_NE(dz::INVALID_ENTRY_INDEX, dirIndex);
    ASSERT_NE(dz::INVALID_ENTRY_INDEX, binIndex);
    ASSERT_NE(dz::INVALID_ENTRY_INDEX, txtIndex);
    ASSERT_EQ(dz::INVALID_ENTRY_INDEX, invIndex);
    ASSERT_NE(dirIndex, binIndex);
    ASSERT_NE(dirIndex, txtIndex);
    ASSERT_NE(binIndex, txtIndex);
    ASSERT_EQ(dirName, archive.EntryName(dirIndex));
    ASSERT_EQ(binName, archive.EntryName(binIndex));
    ASSERT_EQ(txtName, archive.EntryName(txtIndex));
    ASSERT_THROW(archive.EntryName(invIndex), dz::Exception);
    ASSERT_TRUE(archive.IsDirEntry(dirIndex));
    ASSERT_FALSE(archive.IsDirEntry(binIndex));
    ASSERT_FALSE(archive.IsDirEntry(txtIndex));
    ASSERT_THROW(archive.IsDirEntry(invIndex), dz::Exception);

    // Extract entire archive
    {
        du::TempDir tempDir;
        archive.ExtractAll(tempDir.Path());
        const auto dirExtracted = tempDir.Path() / dirName;
        const auto binExtracted = tempDir.Path() / binName;
        const auto txtExtracted = tempDir.Path() / txtName;
        ASSERT_TRUE(fs::exists(dirExtracted));
        ASSERT_TRUE(fs::exists(binExtracted));
        ASSERT_TRUE(fs::exists(txtExtracted));
        ASSERT_TRUE(fs::is_directory(dirExtracted));
        ASSERT_TRUE(fs::is_regular_file(binExtracted));
        ASSERT_TRUE(fs::is_regular_file(txtExtracted));
        ASSERT_EQ(binSize, fs::file_size(binExtracted));
        ASSERT_EQ(txtSize, fs::file_size(txtExtracted));
        ASSERT_THROW(archive.ExtractFileTo(binIndex, tempDir.Path()/"nonexistent"), std::runtime_error);
    }

    // Extract individual entries
    {
        du::TempDir tempDir;
        const auto binExtracted = archive.ExtractFileTo(binIndex, tempDir.Path());
        const auto txtExtracted = archive.ExtractFileTo(txtIndex, tempDir.Path());
        ASSERT_EQ(0, binExtracted.compare(tempDir.Path() / binFilename));
        ASSERT_EQ(0, txtExtracted.compare(tempDir.Path() / txtFilename));
        ASSERT_EQ(binSize, fs::file_size(binExtracted));
        ASSERT_EQ(txtSize, fs::file_size(txtExtracted));
        ASSERT_THROW(archive.ExtractFileTo(invIndex, tempDir.Path()), dz::Exception);
        ASSERT_THROW(archive.ExtractFileTo(binIndex, tempDir.Path()/"nonexistent"), std::runtime_error);
    }

    archive.Discard();
    ASSERT_FALSE(archive.IsOpen());
    ASSERT_NO_THROW(archive.Discard());
}
