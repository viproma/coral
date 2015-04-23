#include <cassert>
#include <iostream>
#include <string>

#include "gtest/gtest.h"
#include "test_common.hpp"


std::string g_fmuDir = std::string();

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if (argc < 2) {
        std::cerr << "Error: FMU directory not specified" << std::endl;
        return 1;
    }
    g_fmuDir = argv[1];
    return RUN_ALL_TESTS();
}


const std::string& FmuDir()
{
    assert(!g_fmuDir.empty());
    return g_fmuDir;
}


TempDir::TempDir()
    : m_path(boost::filesystem::temp_directory_path()
             / boost::filesystem::unique_path())
{
    boost::filesystem::create_directory(m_path);
}

TempDir::~TempDir()
{
    boost::system::error_code ec;
    boost::filesystem::remove_all(m_path, ec);
}

const boost::filesystem::path& TempDir::Path() const
{
    return m_path;
}
