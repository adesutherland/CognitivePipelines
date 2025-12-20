#include <gtest/gtest.h>

#include "test_app.h"

int main(int argc, char** argv)
{
    sharedTestApp(argc, argv);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
