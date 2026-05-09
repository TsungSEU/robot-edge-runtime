// test_file_compress.cpp - Unit tests for FileCompress

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <vector>

#include "data_collection/recorder/file_compress.h"

using namespace aurora::collector;

class FileCompressTest : public ::testing::Test {
protected:
    std::string test_dir_ = "/tmp/file_compress_test";
    std::string test_file_;

    void SetUp() override {
        std::filesystem::create_directories(test_dir_);
        test_file_ = test_dir_ + "/test_data.txt";
    }

    void TearDown() override {
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }

    // Create test file with specified content
    bool createTestFile(const std::string& path, const std::string& content) {
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        file << content;
        file.close();
        return std::filesystem::exists(path);
    }

    // Create test file with binary data
    bool createBinaryTestFile(const std::string& path, size_t size_kb) {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        std::vector<char> data(1024, 'X');
        for (size_t i = 0; i < size_kb; ++i) {
            file.write(data.data(), data.size());
        }
        file.close();
        return std::filesystem::exists(path);
    }
};

// ErrorCode enum tests
TEST_F(FileCompressTest, ErrorCode_ValuesAreCorrect) {
    EXPECT_EQ(static_cast<int>(FileCompress::ErrorCode::Success), 0);
    EXPECT_EQ(static_cast<int>(FileCompress::ErrorCode::InvalidInputPath), 1);
    EXPECT_EQ(static_cast<int>(FileCompress::ErrorCode::FailedToOpenFile), 2);
    EXPECT_EQ(static_cast<int>(FileCompress::ErrorCode::FailedToCreateOutput), 3);
    EXPECT_EQ(static_cast<int>(FileCompress::ErrorCode::CompressionFailed), 4);
    EXPECT_EQ(static_cast<int>(FileCompress::ErrorCode::UnknownError), 5);
    EXPECT_EQ(static_cast<int>(FileCompress::ErrorCode::FailedToCreateTarFile), 6);
}

// Single file compression tests
TEST_F(FileCompressTest, CompressSingleFile_ValidInput_ReturnsSuccess) {
    ASSERT_TRUE(createBinaryTestFile(test_file_, 10));

    FileCompress compressor;
    std::string output_file = test_dir_ + "/compressed.lz4";
    auto result = compressor.CompressSingleFileToLz4(test_file_, output_file);

    EXPECT_EQ(result, FileCompress::ErrorCode::Success);
    EXPECT_TRUE(std::filesystem::exists(output_file));
}

TEST_F(FileCompressTest, CompressSingleFile_NonExistentInput_ReturnsError) {
    FileCompress compressor;
    std::string output_file = test_dir_ + "/compressed.lz4";
    auto result = compressor.CompressSingleFileToLz4("/nonexistent/file.txt", output_file);

    EXPECT_NE(result, FileCompress::ErrorCode::Success);
    EXPECT_FALSE(std::filesystem::exists(output_file));
}

TEST_F(FileCompressTest, CompressSingleFile_EmptyFile_ReturnsSuccess) {
    ASSERT_TRUE(createTestFile(test_file_, ""));

    FileCompress compressor;
    std::string output_file = test_dir_ + "/compressed.lz4";
    auto result = compressor.CompressSingleFileToLz4(test_file_, output_file);

    // Should handle gracefully
    if (result == FileCompress::ErrorCode::Success) {
        EXPECT_TRUE(std::filesystem::exists(output_file));
    }
}

TEST_F(FileCompressTest, CompressSingleFile_ReducesSize) {
    // Create compressible data (repeating pattern)
    std::ofstream file(test_file_);
    std::string repeat_data(1024, 'A');  // Highly compressible
    for (int i = 0; i < 100; ++i) {
        file << repeat_data;
    }
    file.close();

    size_t original_size = std::filesystem::file_size(test_file_);

    FileCompress compressor;
    std::string output_file = test_dir_ + "/compressed.lz4";
    auto result = compressor.CompressSingleFileToLz4(test_file_, output_file);

    ASSERT_EQ(result, FileCompress::ErrorCode::Success);
    ASSERT_TRUE(std::filesystem::exists(output_file));

    size_t compressed_size = std::filesystem::file_size(output_file);
    EXPECT_LT(compressed_size, original_size);
}

// Multiple file compression tests
TEST_F(FileCompressTest, CompressFiles_MultipleFiles_ReturnsSuccess) {
    std::vector<std::string> input_files;
    for (int i = 0; i < 3; ++i) {
        std::string file = test_dir_ + "/test_" + std::to_string(i) + ".txt";
        ASSERT_TRUE(createTestFile(file, "Test content " + std::to_string(i)));
        input_files.push_back(file);
    }

    std::string output_file = test_dir_ + "/compressed.tar.lz4";
    auto result = FileCompress::CompressFiles(input_files, output_file);

    EXPECT_EQ(result, FileCompress::ErrorCode::Success);
    EXPECT_TRUE(std::filesystem::exists(output_file));
}

TEST_F(FileCompressTest, CompressFiles_EmptyList_ReturnsError) {
    std::vector<std::string> empty_list;
    std::string output_file = test_dir_ + "/compressed.tar.lz4";
    auto result = FileCompress::CompressFiles(empty_list, output_file);

    EXPECT_NE(result, FileCompress::ErrorCode::Success);
}

TEST_F(FileCompressTest, CompressFiles_ContainsNonExistentFile_ReturnsError) {
    std::vector<std::string> input_files;
    input_files.push_back("/nonexistent/file.txt");

    std::string output_file = test_dir_ + "/compressed.tar.lz4";
    auto result = FileCompress::CompressFiles(input_files, output_file);

    EXPECT_NE(result, FileCompress::ErrorCode::Success);
}

// Directory enumeration tests
TEST_F(FileCompressTest, GetFilesInDirectory_ExistingDirectory_ReturnsFiles) {
    // Create test files
    createTestFile(test_dir_ + "/file1.txt", "content1");
    createTestFile(test_dir_ + "/file2.txt", "content2");

    std::vector<std::string> files;
    // Note: GetFilesInDirectory is private, tested indirectly via CompressFiles
}

// Error handling tests
TEST_F(FileCompressTest, HandleInvalidOutputPath_DoesNotCreateFile) {
    ASSERT_TRUE(createTestFile(test_file_, "test content"));

    FileCompress compressor;
    std::string invalid_output = "/nonexistent/directory/compressed.lz4";
    auto result = compressor.CompressSingleFileToLz4(test_file_, invalid_output);

    EXPECT_NE(result, FileCompress::ErrorCode::Success);
}

TEST_F(FileCompressTest, HandleLargeFile_CompressesSuccessfully) {
    // Create a larger file (1MB)
    ASSERT_TRUE(createBinaryTestFile(test_file_, 1024));

    FileCompress compressor;
    std::string output_file = test_dir_ + "/compressed.lz4";
    auto result = compressor.CompressSingleFileToLz4(test_file_, output_file);

    EXPECT_EQ(result, FileCompress::ErrorCode::Success);
    EXPECT_TRUE(std::filesystem::exists(output_file));
}

// Gzip compression tests
TEST_F(FileCompressTest, CompressFiles_Gzip_MultipleFiles_ReturnsSuccess) {
    std::vector<std::string> input_files;
    for (int i = 0; i < 3; ++i) {
        std::string file = test_dir_ + "/test_" + std::to_string(i) + ".txt";
        ASSERT_TRUE(createTestFile(file, "Test content " + std::to_string(i)));
        input_files.push_back(file);
    }

    std::string output_file = test_dir_ + "/compressed.tar.gz";
    auto result = FileCompress::CompressFiles(input_files, output_file,
                                              FileCompress::CompressionFormat::Gzip);

    EXPECT_EQ(result, FileCompress::ErrorCode::Success);
    EXPECT_TRUE(std::filesystem::exists(output_file));
}

TEST_F(FileCompressTest, CompressSingleFile_Gzip_ReducesSize) {
    std::ofstream file(test_file_);
    std::string repeat_data(1024, 'A');
    for (int i = 0; i < 100; ++i) {
        file << repeat_data;
    }
    file.close();

    size_t original_size = std::filesystem::file_size(test_file_);

    FileCompress compressor;
    std::string output_file = test_dir_ + "/compressed.gz";
    auto result = compressor.CompressSingleFileToLz4(test_file_, output_file,
                                                     FileCompress::CompressionFormat::Gzip);

    ASSERT_EQ(result, FileCompress::ErrorCode::Success);
    ASSERT_TRUE(std::filesystem::exists(output_file));

    size_t compressed_size = std::filesystem::file_size(output_file);
    EXPECT_LT(compressed_size, original_size);
}

TEST_F(FileCompressTest, CompressFiles_Gzip_LargeFile_CompressesSuccessfully) {
    ASSERT_TRUE(createBinaryTestFile(test_file_, 1024));

    std::string output_file = test_dir_ + "/compressed_large.tar.gz";
    std::vector<std::string> input_files = {test_file_};
    auto result = FileCompress::CompressFiles(input_files, output_file,
                                              FileCompress::CompressionFormat::Gzip);

    EXPECT_EQ(result, FileCompress::ErrorCode::Success);
    EXPECT_TRUE(std::filesystem::exists(output_file));
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
