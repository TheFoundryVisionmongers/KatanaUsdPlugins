#include <iostream>

#include <FnAttribute/FnAttribute.h>
#include "gtest/gtest.h"

#include "usdKatana/bootstrap.h"
#include "vtKatana/bootstrap.h"

int main(int argc, char* argv[])
{
    char* katanaRoot = getenv("KATANA_ROOT");
    if (katanaRoot == nullptr)
    {
        std::cerr << "KATANA_ROOT is not set!" << std::endl;
        return 0;
    }

    if (!FnAttribute::Bootstrap(getenv("KATANA_ROOT")))
    {
        std::cerr << "Failed to bootstrap FnAttribute" << std::endl;
        return 0;
    }
    else
    {
        auto suite = FnAttribute::Attribute::getSuite();
        FnAttribute::Initialize(suite);
    }

    PXR_NS::UsdKatanaBootstrap(katanaRoot);
    PXR_NS::VtKatanaBootstrap(katanaRoot);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
