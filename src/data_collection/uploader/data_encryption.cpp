//
// Created by xucong on 25-5-8.
// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
// Tsung Xu<congx0829@163.com>
//

#include "data_encryption.h"

#include <chrono>
#include <iomanip>

#include "common/log/logger.h"
#include "common/utils/utils.h"
#include "common/utils/sRegex.h"

namespace aurora::collector {

DataEncryption::~DataEncryption() {
    free_keys();
}

void DataEncryption::free_keys() {
    if (public_key) EVP_PKEY_free(public_key);
    if (private_key) EVP_PKEY_free(private_key);
    public_key = private_key = nullptr;
}

bool DataEncryption::Init(const std::string &cloud_pubkey_file_path, const std::string& watch_dir, const std::string& enc_dir) {
    stop_flag_ = false;
    encrypt_paths.emplace("encryptPath", watch_dir);
    enc_dir_ = enc_dir;
    std::cout << "encryptPath: " << watch_dir  << std::endl;
    std::cout << "enc_dir_: " << enc_dir_   << std::endl;
    // 加载公钥
    BIO* bio = BIO_new_file(cloud_pubkey_file_path.c_str(), "r");
    if (!bio) return false;

    std::cout << "cloud_pubkey_file_path:" << cloud_pubkey_file_path <<std::endl;

    public_key = PEM_read_bio_PUBKEY(bio, nullptr,
                                     nullptr, nullptr);
    // std::cout << "PEM_read_bio_PUBKEY finish" << std::endl;
    // std::cout << "1:" << (public_key != nullptr)  << std::endl;

    if (public_key == nullptr) {
        const char *err = ERR_error_string(ERR_get_error(), NULL);
        fprintf(stderr, "Error reading public key: %s\n", err);
    }

    BIO_free_all(bio);
    // std::cout << "BIO_free_all" << std::endl;
    // std::cout << (public_key != nullptr)  << std::endl;
    return public_key != nullptr;
}

bool DataEncryption::Start()
{
    worker_thread_ = std::thread(&DataEncryption::Run, this);

    return true;
}

void DataEncryption::GetFolderName(std::string& folder_name) {
    // 获取当前系统时间
    auto now = std::chrono::system_clock::now();

    // 将时间转换为time_t类型，以便使用gmtime
    auto now_time_t = std::chrono::system_clock::to_time_t(now);

    // 使用gmtime将time_t转换为struct tm
    struct tm time_info;
    gmtime_r(&now_time_t, &time_info);  // 使用本地时间

    // 定义文件名格式，包含年、月、日
    const char* filename_format = "%Y%m%d";

    // 创建一个字符串流来构建文件名
    std::ostringstream filename_stream;
    filename_stream << std::put_time(&time_info, filename_format);
    // 将 ostringstream 转换为 string
    folder_name= filename_stream.str();
    // folder_name = "test";
}

//加载指定目录中的文件列表，并将符合条件的文件路径加入到upload_queue中
void DataEncryption::LoadFileList() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string folder_name;
    GetFolderName(folder_name);
    AD_INFO(DataEncryption, "upload_folder_name_today: %s", folder_name.c_str());
    for (const auto& [_, upload_dir] : encrypt_paths) {
        std::string upload_dir_today = upload_dir + "/"+ folder_name;
        AD_INFO(DataEncryption, "upload_dir_today: %s", upload_dir_today.c_str());
        if (!common::IsDirExist(upload_dir_today)) {
            AD_ERROR(DataEncryption, "Directory %s does not exist.", upload_dir_today.c_str());
            continue;
        }
        for (const auto& entry : fs::directory_iterator(upload_dir_today)) {
            if (entry.is_regular_file() && common::IsMatch(entry.path().filename().string(), "(\\.(zip|ZIP)$)")) {
                encrypt_queue_.push(entry.path().string());
            }
        }
    }
    AD_INFO(DataEncryption, "Loaded %d files from encrypt paths.", encrypt_queue_.size());
}

void DataEncryption::Run() {
    AD_INFO(DataEncryption, "Encrypt Run.");
    while (!stop_flag_) {
        LoadFileList();
        ProcessQueue();
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::seconds(1));
    }
}

//从队列中抽出文件进行加密，成功则删除文件，失败则重试上传
void DataEncryption::ProcessQueue() {
    while (!stop_flag_) {
        std::string current_file;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (encrypt_queue_.empty()) {
                AD_INFO(DataEncryption, "No files in queue.");
                break;
            }
            current_file = encrypt_queue_.front();
            encrypt_queue_.pop();
        }

        std::filesystem::path current_file_path(current_file);
        std::string encrypted_file = enc_dir_ + "/" + current_file_path.filename().string() + ".enc";

        // std::string encrypted_file = current_file + ".enc";
        std::cout  << "Encrypting file:   " << encrypted_file << std::endl;

        auto success = EncryptFileWithEnvelope(current_file, encrypted_file);
        AD_INFO(DataEncryption, "success: %d", success);

        if (success == 0) {
            AD_INFO(DataEncryption, "Uploaded file: %s", current_file.c_str());
            common::DeleteFile(current_file);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else {
            std::lock_guard<std::mutex> lock(mutex_);
            AD_ERROR(DataEncryption, "Failed to upload file: %s", current_file.c_str());
            encrypt_queue_.push(current_file);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

bool DataEncryption::Stop() {
    AD_INFO(DataEncryption, "Stop.");
    stop_flag_ = true;
    cv_.notify_all();
    return true;
}

bool DataEncryption::load_private_key(const std::string& priv_key_path) {
    FILE* fp = fopen(priv_key_path.c_str(), "r");
    if (!fp) {
        error_msg = "cannot open private key file";
        return false;
    }

    private_key = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
    fclose(fp);

    if (!private_key) {
        error_msg = "fail to read private key!";
        return false;
    }
    return true;
}
// AES-256-CBC 加密
std::vector<unsigned char> DataEncryption::aes_encrypt(const std::vector<unsigned char>& plaintext) {
    if (aes_key.empty() || iv.empty()) return {};

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    std::vector<unsigned char> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
    int len, ciphertext_len = 0;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                           aes_key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                          (const unsigned char*)plaintext.data(), plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    ciphertext_len = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    ciphertext.resize(ciphertext_len);

    // 将IV添加到密文前
    std::vector<unsigned char> full_output = iv;
    full_output.insert(full_output.end(), ciphertext.begin(), ciphertext.end());
    return full_output;
}


// 打印十六进制数据
void DataEncryption::print_hex(const std::string& label, const unsigned char* data, size_t length) {
    std::cout << label << ": " << length << " bytes, hex: ";
    auto end = length < 32 ? length:32;
    std::cout << "end:" << end << std::endl;
    for (size_t i = 0; i < end; ++i) { // 只打印前 16 字节
        // std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
        std::cout << "i:" << i << "data: "  << (unsigned int)(data[i])<< " ";
    }
    if (length > 32) {
        std::cout << "...";
    }
    std::cout << std::endl;
}

// RSA加密
std::vector<unsigned char> DataEncryption::rsa_encrypt(const std::string& plaintext) {
    if (!public_key) return {};

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(public_key, nullptr);
    if (!ctx) return {};

    size_t outlen;
    if (EVP_PKEY_encrypt_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return {};
    }

    if (EVP_PKEY_encrypt(ctx, nullptr, &outlen,
                         (const unsigned char*)plaintext.data(),
                         plaintext.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return {};
    }

    std::vector<unsigned char> ciphertext(outlen);
    if (EVP_PKEY_encrypt(ctx, ciphertext.data(), &outlen,
                         (const unsigned char*)plaintext.data(),
                         plaintext.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return {};
    }

    EVP_PKEY_CTX_free(ctx);
    ciphertext.resize(outlen);
    return ciphertext;
}

// 加密文件
int DataEncryption::EncryptDataWithEnvelope(const std::string &plaintext, std::string &ciphertext) {
    // 1. Generate AES-256 key (32 bytes) and random IV (16 bytes)
    aes_key.resize(32);
    if (RAND_bytes(aes_key.data(), aes_key.size()) != 1) {
        std::cerr << "Failed to generate AES key: " << ERR_error_string(ERR_get_error(), NULL) << std::endl;
        return -1;
    }
    iv.resize(16);
    if (RAND_bytes(iv.data(), iv.size()) != 1) {
        std::cerr << "Failed to generate IV: " << ERR_error_string(ERR_get_error(), NULL) << std::endl;
        return -1;
    }

    // 2. Encrypt the data with AES-256-CBC
    std::vector<unsigned char> plaintext_vec(plaintext.begin(), plaintext.end());
    auto aes_ciphertext = aes_encrypt(plaintext_vec);
    if (aes_ciphertext.empty()) {
        error_msg = "AES encryption failed";
        return -1;
    }

    // 3. Encrypt the AES key with RSA
    std::string aes_key_str(aes_key.begin(), aes_key.end());
    auto encrypted_key = rsa_encrypt(aes_key_str);
    if (encrypted_key.empty()) {
        error_msg = "RSA encryption failed";
        return -1;
    }

    // print_hex("aes_key:", aes_key.data(), aes_key.size());
    // print_hex("encrypted_key:", encrypted_key.data(), encrypted_key.size());

    // 4. Prepare output structure:
    //    [encrypted key length (4 bytes)][encrypted key][IV (16 bytes)][encrypted data]
    auto combined = combine_encrypted_data(encrypted_key, aes_ciphertext);
    ciphertext.assign(combined.begin(), combined.end());

    return 0;
}

// RSA分片加密
int DataEncryption::rsa_chunk_encrypt(std::ifstream& in_file, std::ofstream& out_file) {
    struct EVP_CTX_Deleter {
        void operator()(EVP_CIPHER_CTX* ctx) { EVP_CIPHER_CTX_free(ctx); }
    };
    std::unique_ptr<EVP_CIPHER_CTX, EVP_CTX_Deleter> ctx_ptr(EVP_CIPHER_CTX_new());
    if (!ctx_ptr) {
        std::cerr << "Failed to create cipher context!" << std::endl;
        return -1;
    }

    EVP_CIPHER_CTX* ctx = ctx_ptr.get();
    // 初始化加密操作
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, aes_key.data(), iv.data()) != 1) {
        std::cerr << "Failed to initialize encryption!" << std::endl;
        return -1;
    }

    // 将IV写入输出文件
    out_file.write(reinterpret_cast<const char*>(iv.data()), 16);


    size_t block_size = 1024 * 1024;
    unsigned char in_buffer[block_size];
    unsigned char out_buffer[block_size + EVP_MAX_BLOCK_LENGTH]; // 确保有足够的空间存放加密数据和填充

    while (in_file.read(reinterpret_cast<char*>(in_buffer), block_size)) {
        int len;
        if (EVP_EncryptUpdate(ctx, out_buffer, &len, in_buffer, static_cast<int>(block_size)) != 1) {
            std::cerr << "Failed to encrypt data!" << std::endl;
            return -1;
        }
        out_file.write(reinterpret_cast<const char*>(out_buffer), len);
    }

    size_t remaining = in_file.gcount();
    if (remaining > 0) { // 对剩余的数据进行加密
        int len, final_len;
        if (EVP_EncryptUpdate(ctx, out_buffer, &len, in_buffer, static_cast<int>(remaining)) != 1 ||
            EVP_EncryptFinal_ex(ctx, out_buffer + len, &final_len) != 1) {
            std::cerr << "Failed to finalize encryption!" << std::endl;
            return -1;
        }
        out_file.write(reinterpret_cast<const char*>(out_buffer), len + final_len);
    }

    return 0;
}

int DataEncryption::EncryptFileWithEnvelope(const std::string &plainfile, const std::string &cipherfile) {
    //1.读取文件内容
    std::ifstream in_file(plainfile, std::ios::binary);
    if (!in_file) {
        error_msg = "cannot open input file";
        return -1;
    }

    std::string plaintext(
        (std::istreambuf_iterator<char>(in_file)),
        std::istreambuf_iterator<char>()
    );
    in_file.close();

    //2.加密数据
    std::string ciphertext;
    if (EncryptDataWithEnvelope(plaintext, ciphertext) != 0) {
        return -1;
    }

    //3.写入文件
    std::ofstream out_file(cipherfile, std::ios::binary);
    if (!out_file) {
        error_msg = "cannot create output file";
        return -1;
    }
    out_file.write(ciphertext.data(), ciphertext.size());
    out_file.close();

    return 0;

}

int DataEncryption::DecryptFileWithEnvelope(const std::string& cipherfile, const std::string& plainfile){
    std::ifstream in_file(cipherfile , std::ios::binary);
    if (!in_file) {
        error_msg = "cannot open input file";
        return -1;
    }

    std::string ciphertext_str(
        (std::istreambuf_iterator<char>(in_file)),
        std::istreambuf_iterator<char>()
    );
    in_file.close();

    std::cout << cipherfile << " : " << plainfile << " encrypted successfully 1!" << "\n\n";

    std::vector<unsigned char> ciphertext_with_iv(ciphertext_str.begin(), ciphertext_str.end());

    if (aes_key.empty() || ciphertext_with_iv.size() < 16) return -1;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    std::vector<unsigned char> plaintext(ciphertext_with_iv.size() + EVP_MAX_BLOCK_LENGTH);
    int len, plaintext_len = 0;

    // 分离IV和密文
    std::vector<unsigned char> key_len(ciphertext_with_iv.begin(), ciphertext_with_iv.begin()+4);

    int result = 0;
    for (size_t i = 0; i < sizeof(int); ++i) {
        result |= static_cast<int>(key_len[i]) << (8 * (sizeof(int) - 1 - i));
    }

    std::cout << "key len: " << result  << std::endl;

    std::vector<unsigned char> aes_key_file(ciphertext_with_iv.begin()+4, ciphertext_with_iv.begin()+4+32);
    std::vector<unsigned char> iv(ciphertext_with_iv.begin()+4+32, ciphertext_with_iv.begin()+4+32+16);
    std::vector<unsigned char> ciphertext(ciphertext_with_iv.begin()+4+32+16, ciphertext_with_iv.end());

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                           aes_key_file.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len,
                          (const unsigned char*)ciphertext.data(), ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    plaintext_len = len;

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    std::string plaintext_str(plaintext.begin(), plaintext.begin() + plaintext_len);

    //3.写入文件
    std::ofstream out_file(plainfile, std::ios::binary);
    if (!out_file) {
        error_msg = "cannot create output file";
        return -1;
    }
    out_file.write(plaintext_str.data(), plaintext_str.size());
    out_file.close();
    std::cout << cipherfile << " : " << plainfile << " encrypted successfully!" << "\n\n";
    return 0;
}

int DataEncryption::EncryptChunkFileWithEnvelope(const std::string &plainfile, const std::string &cipherfile) {
    //1.读取文件内容
    std::ifstream in_file(plainfile, std::ios::binary);
    if (!in_file) {
        error_msg = "cannot open input file";
        return -1;
    }

    std::ofstream out_file(cipherfile, std::ios::binary);
    if (!out_file) {
        error_msg = "cannot create output file";
        return -1;
    }

    //2.加密数据
    //2.1. Generate AES-256 key (32 bytes) and random IV (16 bytes)
    aes_key.resize(32);
    if (RAND_bytes(aes_key.data(), aes_key.size()) != 1) {
        std::cerr << "Failed to generate AES key: " << ERR_error_string(ERR_get_error(), NULL) << std::endl;
        return -1;
    }
    iv.resize(16);
    if (RAND_bytes(iv.data(), iv.size()) != 1) {
        std::cerr << "Failed to generate IV: " << ERR_error_string(ERR_get_error(), NULL) << std::endl;
        return -1;
    }

    //2.2. Encrypt the AES key with RSA
    std::string aes_key_str(aes_key.begin(), aes_key.end());
    auto encrypted_key = rsa_encrypt(aes_key_str);
    if (encrypted_key.empty()) {
        error_msg = "RSA encryption failed";
        return -1;
    }
    print_hex("aes_key:", aes_key.data(), aes_key.size());
    print_hex("encrypted_key:", encrypted_key.data(), encrypted_key.size());

    // 添加加密密钥长度（网络字节序）
    uint32_t key_len = htonl(static_cast<uint32_t>(encrypted_key.size()));
    // 将加密密钥长度写入输出文件
    out_file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
    // 添加加密密钥
    out_file.write(reinterpret_cast<const char*>(encrypted_key.data()), encrypted_key.size());
    // 添加IV
    // out_file.write(reinterpret_cast<const char*>(iv.data()), 16);

    //2.3. Encrypt the data with AES-256-CBC
    if(rsa_chunk_encrypt(in_file, out_file) != 0) { return -1;}

    in_file.close();
    out_file.close();

    return 0;

}

std::vector<unsigned char> DataEncryption::combine_encrypted_data(const std::vector<unsigned char> &encryptedKey,
                                                                  const std::vector<unsigned char> &ciphertext) {

    std::vector<unsigned char> combined;

    // 添加加密密钥长度（网络字节序）
    uint32_t key_len = htonl(static_cast<uint32_t>(encryptedKey.size()));
    combined.insert(combined.end(),
                    reinterpret_cast<unsigned char*>(&key_len),
                    reinterpret_cast<unsigned char*>(&key_len) + sizeof(key_len));

    // 添加加密密钥
    combined.insert(combined.end(), encryptedKey.begin(), encryptedKey.end());

    // 添加IV
    combined.insert(combined.end(), iv.begin(), iv.end());

    // 添加加密数据
    combined.insert(combined.end(), ciphertext.begin(), ciphertext.end());

    return combined;
}

}
