#include <iostream>

#include <FnAttribute/FnAttribute.h>
#include "gtest/gtest.h"

#include "usdKatana/bootstrap.h"
#include "vtKatana/bootstrap.h"

int main(int argc, char* argv[])
{
    const char* katanaRoot{getenv("KATANA_ROOT")};
    if (!katanaRoot || !FnAttribute::Bootstrap(katanaRoot))
    {
        std::cerr << "Failed to bootstrap FnAttribute" << std::endl;
        return 0;
    }
    else
    {
        auto suite = FnAttribute::Attribute::getSuite();
        FnAttribute::Initialize(suite);
    }

    PXR_NS::UsdKatanaBootstrap();
    PXR_NS::VtKatanaBootstrap();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
