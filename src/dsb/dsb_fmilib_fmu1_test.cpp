#include "gtest/gtest.h"


#include "boost/filesystem.hpp"
#include "dsb/fmilib/importcontext.hpp"
#include "dsb/fmilib/fmu1.hpp"
#include "dsb/util.hpp"


#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
const std::string fmuDir = STRINGIFY(DSB_TEST_FMU_DIRECTORY);


TEST(dsb_fmilib, Fmu1_import_fmu_cs)
{
    auto context = dsb::fmilib::MakeImportContext(nullptr, jm_log_level_nothing);
    dsb::util::TempDir tmpDir;

    const std::string modelName = "compute";
    auto fmu = context->Import(
        (boost::filesystem::path(fmuDir) / "fmi1_cs" / (modelName+".fmu")).string(),
        tmpDir.Path().string());

    ASSERT_EQ(dsb::fmilib::kFmiVersion1_0, fmu->FmiVersion());
    EXPECT_EQ(modelName, fmu->ModelName());
    EXPECT_EQ("2486324958", fmu->GUID());
    EXPECT_TRUE(fmu->Description().empty());
    EXPECT_TRUE(fmu->Author().empty());
    EXPECT_TRUE(fmu->ModelVersion().empty());
    EXPECT_EQ("ControlBuild", fmu->GenerationTool());
    EXPECT_NE(nullptr, std::static_pointer_cast<dsb::fmilib::Fmu1>(fmu)->Handle());
}
