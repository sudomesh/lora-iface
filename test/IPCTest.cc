#include "../ipc.c"
#include <gtest/gtest.h>

TEST(IPCTest, RemoveNonExistentClient) {
  ASSERT_EQ(-1, remove_uclient(0));
}
