#include <gtest/gtest.h>
#include "utils/FileManager.h"
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
    
    // Normalize paths to generic format (forward slashes) for cross-platform comparison
    std::map<std::string, std::string> normalizedFiles;
    for (const auto& [p, c] : files) {
        normalizedFiles[fs::path(p).generic_string()] = c;
    }
    
    std::string f1Path = fs::path(testDir + "/file1.txt").generic_string();
    EXPECT_EQ(normalizedFiles.size(), 3);
    EXPECT_NE(normalizedFiles.find(f1Path), normalizedFiles.end());
    EXPECT_EQ(normalizedFiles[f1Path], "hello world");
}

TEST_F(FileManagerTest, SearchesContentCorrectly) {
    FileManager fm(testDir);
    auto matches = fm.searchFiles("agent");
    
    EXPECT_EQ(matches.size(), 1);
    EXPECT_EQ(fs::path(matches[0]).generic_string(), fs::path(testDir + "/file2.md").generic_string());
}
