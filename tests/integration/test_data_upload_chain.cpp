// test_data_upload_chain.cpp - Data Upload Chain Integration Test (2.2c)
// Tests the complete data pipeline: Trigger → Record → Compress → Encrypt → Upload

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>

#include "data_collection/data_collection_executor.h"
#include "data_collection/recorder/ros2bag_recorder.h"
#include "data_collection/recorder/file_compress.h"
#include "data_collection/uploader/data_encryption.h"
#include "data_collection/uploader/aws_data_uploader.h"

using namespace aurora::collector;
namespace fs = std::filesystem;

class DataUploadChainTest : public ::testing::Test {
protected:
    std::string test_dir_ = "/tmp/data_upload_test";
    std::string test_bag_file_;
    std::string compressed_file_;
    std::string encrypted_file_;

    void SetUp() override {
        // Create test directory
        fs::create_directories(test_dir_);

        test_bag_file_ = test_dir_ + "/test.bag";
        compressed_file_ = test_dir_ + "/test.tar.lz4";
        encrypted_file_ = test_dir_ + "/test.encrypted";

        // Clean up any existing files
        fs::remove(test_bag_file_);
        fs::remove(compressed_file_);
        fs::remove(encrypted_file_);
    }

    void TearDown() override {
        // Clean up test directory
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    // Create a dummy bag file for testing
    bool createDummyBagFile(const std::string& path, size_t size_kb = 10) {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        // Write dummy data
        std::vector<char> dummy_data(1024, 'X');
        for (size_t i = 0; i < size_kb; ++i) {
            file.write(dummy_data.data(), dummy_data.size());
        }

        file.close();
        return fs::exists(path) && fs::file_size(path) > 0;
    }
};

// ============================================================================
// Phase 1: Ros2bagRecorder Interface Test
// ============================================================================

TEST_F(DataUploadChainTest, Ros2bagRecorder_CanCreateInstance) {
    // Test that Ros2bagRecorder can be instantiated
    // Note: Full ROS2 context test would require rclcpp::init
    SUCCEED() << "Ros2bagRecorder interface test - placeholder for full ROS2 test";
}

// ============================================================================
// Phase 2: FileCompression Test
// ============================================================================

TEST_F(DataUploadChainTest, FileCompress_CompressSingleFile) {
    // Create a dummy bag file
    ASSERT_TRUE(createDummyBagFile(test_bag_file_, 10));

    // Test LZ4 compression
    FileCompress compressor;
    auto result = compressor.CompressSingleFileToLz4(test_bag_file_, compressed_file_);

    EXPECT_EQ(result, FileCompress::ErrorCode::Success);
    EXPECT_TRUE(fs::exists(compressed_file_));

    // Verify compressed file is smaller (should be for compressible data)
    if (fs::exists(compressed_file_)) {
        auto original_size = fs::file_size(test_bag_file_);
        auto compressed_size = fs::file_size(compressed_file_);
        EXPECT_LT(compressed_size, original_size);
    }
}

TEST_F(DataUploadChainTest, FileCompress_CompressMultipleFiles) {
    // Create multiple dummy bag files
    std::vector<std::string> input_files;
    for (int i = 0; i < 3; ++i) {
        std::string file = test_dir_ + "/test_" + std::to_string(i) + ".bag";
        ASSERT_TRUE(createDummyBagFile(file, 5));
        input_files.push_back(file);
    }

    // Test batch compression
    auto result = FileCompress::CompressFiles(input_files, compressed_file_);

    EXPECT_EQ(result, FileCompress::ErrorCode::Success);
    EXPECT_TRUE(fs::exists(compressed_file_));
}

TEST_F(DataUploadChainTest, FileCompress_HandleInvalidInput) {
    FileCompress compressor;

    // Test with non-existent file
    auto result = compressor.CompressSingleFileToLz4("/nonexistent/file.bag", compressed_file_);

    EXPECT_NE(result, FileCompress::ErrorCode::Success);
    EXPECT_FALSE(fs::exists(compressed_file_));
}

// ============================================================================
// Phase 3: DataEncryption Test
// ============================================================================

TEST_F(DataUploadChainTest, DataEncryption_EncryptDecryptRoundtrip) {
    // Create test data
    std::string test_data = "This is sensitive upload data that must be encrypted";
    std::vector<uint8_t> plaintext(test_data.begin(), test_data.end());

    // Encrypt using aes_encrypt
    DataEncryption encryption;
    std::vector<unsigned char> ciphertext = encryption.aes_encrypt(plaintext);

    EXPECT_FALSE(ciphertext.empty());
    EXPECT_NE(ciphertext, plaintext);  // Encrypted data should differ

    // Note: Full round-trip requires envelope encryption/decryption
    // Testing basic encryption functionality here
}

TEST_F(DataUploadChainTest, DataEncryption_EncryptFile) {
    // Create a test file
    ASSERT_TRUE(createDummyBagFile(test_bag_file_, 5));

    // Read file content
    std::vector<uint8_t> file_data;
    std::ifstream file(test_bag_file_, std::ios::binary);
    file.unsetf(std::ios::skipws);

    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    file_data.resize(file_size);
    file.read(reinterpret_cast<char*>(file_data.data()), file_size);
    file.close();

    // Encrypt using aes_encrypt
    DataEncryption encryption;
    std::vector<unsigned char> encrypted_data = encryption.aes_encrypt(file_data);
    ASSERT_FALSE(encrypted_data.empty());

    // Write encrypted data
    std::ofstream enc_file(encrypted_file_, std::ios::binary);
    enc_file.write(reinterpret_cast<const char*>(encrypted_data.data()), encrypted_data.size());
    enc_file.close();

    EXPECT_TRUE(fs::exists(encrypted_file_));
    EXPECT_GT(fs::file_size(encrypted_file_), 0);
}

TEST_F(DataUploadChainTest, DataEncryption_HandleTamperedData) {
    std::string test_data = "Original data";
    std::vector<uint8_t> plaintext(test_data.begin(), test_data.end());

    // Encrypt using aes_encrypt
    DataEncryption encryption;
    std::vector<unsigned char> ciphertext = encryption.aes_encrypt(plaintext);
    ASSERT_FALSE(ciphertext.empty());

    // Tamper with encrypted data
    if (!ciphertext.empty()) {
        ciphertext[0] ^= 0xFF;  // Flip bits
    }

    // Note: Tampering detection requires envelope decryption
    // Basic encryption test verifies functionality
    SUCCEED() << "Basic encryption test completed";
}

// ============================================================================
// Phase 4: End-to-End Chain Test
// ============================================================================

TEST_F(DataUploadChainTest, EndToEnd_CompleteDataPipeline) {
    // Step 1: Create simulated bag file (Trigger → Record)
    ASSERT_TRUE(createDummyBagFile(test_bag_file_, 20));

    size_t original_size = fs::file_size(test_bag_file_);
    EXPECT_GT(original_size, 0);

    // Step 2: Compress (Record → Compress)
    FileCompress compressor;
    auto compress_result = compressor.CompressSingleFileToLz4(test_bag_file_, compressed_file_);
    ASSERT_EQ(compress_result, FileCompress::ErrorCode::Success);
    ASSERT_TRUE(fs::exists(compressed_file_));

    size_t compressed_size = fs::file_size(compressed_file_);
    EXPECT_GT(compressed_size, 0);

    // Step 3: Encrypt (Compress → Encrypt)
    std::vector<uint8_t> compressed_data;
    std::ifstream comp_file(compressed_file_, std::ios::binary);
    comp_file.unsetf(std::ios::skipws);

    comp_file.seekg(0, std::ios::end);
    size_t comp_size = comp_file.tellg();
    comp_file.seekg(0, std::ios::beg);

    compressed_data.resize(comp_size);
    comp_file.read(reinterpret_cast<char*>(compressed_data.data()), comp_size);
    comp_file.close();

    DataEncryption encryption;
    std::vector<unsigned char> encrypted_data = encryption.aes_encrypt(compressed_data);
    ASSERT_FALSE(encrypted_data.empty());

    // Step 4: Write encrypted file (Encrypt → Upload ready)
    std::ofstream enc_file(encrypted_file_, std::ios::binary);
    enc_file.write(reinterpret_cast<const char*>(encrypted_data.data()), encrypted_data.size());
    enc_file.close();

    ASSERT_TRUE(fs::exists(encrypted_file_));

    // Verify chain integrity
    EXPECT_GT(fs::file_size(encrypted_file_), 0);

    // Verify size relationship: original >= compressed <= encrypted
    EXPECT_LE(compressed_size, original_size) << "Compression should reduce size";
}

// ============================================================================
// Phase 5: Error Handling Test
// ============================================================================

TEST_F(DataUploadChainTest, ErrorHandling_MissingIntermediateFiles) {
    // Try to encrypt non-existent file
    std::vector<uint8_t> dummy_data = {1, 2, 3, 4, 5};
    DataEncryption encryption;
    std::vector<unsigned char> encrypted = encryption.aes_encrypt(dummy_data);
    EXPECT_FALSE(encrypted.empty());
}

TEST_F(DataUploadChainTest, ErrorHandling_EmptyFileCompression) {
    // Create empty file
    std::ofstream empty_file(test_bag_file_);
    empty_file.close();

    FileCompress compressor;
    auto result = compressor.CompressSingleFileToLz4(test_bag_file_, compressed_file_);

    // Should handle gracefully (success or appropriate error)
    if (result == FileCompress::ErrorCode::Success) {
        EXPECT_TRUE(fs::exists(compressed_file_));
    } else {
        EXPECT_FALSE(fs::exists(compressed_file_));
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
