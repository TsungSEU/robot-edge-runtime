//
// Created by xucong on 25-5-8.
// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>
//

#ifndef DATA_ENCRYPTION_H
#define DATA_ENCRYPTION_H

#include <iostream>
#include <string>
#include <queue>
#include <vector>
#include <limits>
#include <memory>
#include <fstream>
#include <filesystem>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <map>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>

namespace aurora::collector {

static const char* LOG_TAG = "DATA_ENCRYPTION";
namespace fs = std::filesystem;

class DataEncryption {
public:
    EVP_PKEY* m_cloud_pubkey = nullptr;
    std::map<std::string, std::string> encrypt_paths;
    std::string enc_dir_;

    DataEncryption() = default;
    virtual ~DataEncryption();

    // AES 加密核心函数
    std::vector<unsigned char> aes_encrypt(const std::vector<unsigned char>& plaintext);
    // RSA加密AES密钥
    std::vector<unsigned char> rsa_encrypt(const std::string& plaintext);

    bool Init(const std::string& cloud_pubkey_file_path, const std::string& watch_dir, const std::string& enc_dir);
    bool Start();
    bool Stop();
    bool load_private_key(const std::string& priv_key_path);
    void free_keys();
    void GetFolderName(std::string& folder_name);

    int EncryptDataWithEnvelope(const std::string& plaintext, std::string& ciphertext);
    int DecryptDataWithEnvelope(const std::string& ciphertext, std::string& plaintext);
    int EncryptFileWithEnvelope(const std::string& plainfile, const std::string& cipherfile);
    int DecryptFileWithEnvelope(const std::string& cipherfile, const std::string& plainfile);

    int rsa_chunk_encrypt(std::ifstream& in_file, std::ofstream& out_file);
    int EncryptChunkFileWithEnvelope(const std::string &plainfile, const std::string &cipherfile);

    std::string last_error() const { return error_msg; };

private:
    void Run();
    void LoadFileList();
    void ProcessQueue();
    std::vector<unsigned char> combine_encrypted_data(const std::vector<unsigned char>& encryptedKey,
                                                      const std::vector<unsigned char>& ciphertext);
    void print_hex(const std::string& label, const unsigned char* data, size_t length);

private:
    std::string cloud_pubkey_file_path_;
    std::vector<unsigned char> aes_key;
    std::vector<unsigned char> iv;
    // 使用 EVP_PKEY 管理密钥
    EVP_PKEY* private_key = nullptr;
    EVP_PKEY* public_key = nullptr;
    mutable std::string error_msg;
    std::queue<std::string> encrypt_queue_;
    std::thread worker_thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_flag_;
};

}

#endif //DATA_ENCRYPTION_H
