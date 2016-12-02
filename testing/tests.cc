// tests.cpp
#include "../ipc.c"
#include <gtest/gtest.h>

TEST(IPCTest, RemoveNonExistentClient) {
  ASSERT_EQ(-1, remove_uclient(0));
}
 
int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
