#include <gtest/gtest.h>
#include "../src/FileManager.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

class FileManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = "test_sandbox";
        fs::create_directory(testDir);
        
        std::ofstream f1(testDir + "/file1.txt");
        f1 << "hello world";
        f1.close();
        
        std::ofstream f2(testDir + "/file2.md");
        f2 << "cpp agent test";
        f2.close();

        fs::create_directory(testDir + "/sub");
        std::ofstream f3(testDir + "/sub/file3.json");
        f3 << "{\"key\": \"value\"}";
        f3.close();
    }

    void TearDown() override {
        fs::remove_all(testDir);
    }

    std::string testDir;
};

TEST_F(FileManagerTest, ReadsAllFilesSuccessfully) {
    FileManager fm(testDir);
    auto files = fm.readAllFiles();
    
    EXPECT_EQ(files.size(), 3);
    EXPECT_NE(files.find(testDir + "/file1.txt"), files.end());
    EXPECT_EQ(files[testDir + "/file1.txt"], "hello world");
}

TEST_F(FileManagerTest, SearchesContentCorrectly) {
    FileManager fm(testDir);
    auto matches = fm.searchFiles("agent");
    
    EXPECT_EQ(matches.size(), 1);
    EXPECT_EQ(matches[0], testDir + "/file2.md");
}
