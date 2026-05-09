#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <stdio.h>
#include <curl/curl.h>
#include <string.h> 

namespace aurora::collector {

class CurlWrapper {
public:
    CurlWrapper() : curl_(nullptr) {}
    ~CurlWrapper() {
        if (curl_) {
            curl_easy_cleanup(curl_);
            curl_global_cleanup();
            curl_ = nullptr;
        }
    }
    CURLcode Init(const std::string& client_cert_path = "", const std::string& client_key_path = "", const std::string& ca_cert_path = "");
    CURLcode IsInited() const;
    CURLcode HttpPost(const std::string& url, const std::string& data, std::string& response, const std::vector<std::string>& heads);
    CURLcode HttpGet(const std::string& url, std::string& response, const std::vector<std::string>& heads);
    CURLcode HttpPut(const std::string& url, const std::vector<char>& data, std::string& response, const std::vector<std::string>& heads);

private:
    CURL* curl_;
    std::string client_cert_path_;
    std::string client_key_path_;
    std::string ca_cert_path_;

    CurlWrapper(const CurlWrapper&) = delete;
    CurlWrapper& operator=(const CurlWrapper&) = delete;
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    // 回调函数用于读取和存储服务器的响应数据
    static size_t WriteCallback1(void *ptr, size_t size, size_t nmemb, void *data) {
        size_t written = fwrite(ptr, size, nmemb, (FILE *)data);
        // std::cout << "写入的body：" <<std::endl ;
        fwrite(ptr, size, nmemb , stdout);
        return written;
    }

    // 回调函数用于读取和存储服务器的响应数据
    static size_t WriteHeadCallback1(void *ptr, size_t size, size_t nmemb, void *data) {
        // size_t written = fwrite(ptr, size, nmemb, (FILE *)data);
        std::cout << "写入的头："<< (char*)ptr << "end" << std::endl;
        char *etag = (char *)data;
        // std::cout << "写入的头："<< (char*)ptr << "end" << std::endl;
        // std::cout << "size："<< (int)size << " nmemb：" <<  (int)nmemb  <<std::endl ;


        // std::string str1(str);
        // return str1.find(keyword) != std::string::npos;

        // 检查是否是ETag字段
        if(strstr((char*)ptr, "ETag: ") == ptr) {
            // 复制ETag值到用户数据缓冲区
            strncpy(etag, &((char*)ptr)[7], 32);  // 跳过 "ETag: " 并复制剩余部分
            etag[32] = '\0';  // 确保以空字符结尾
            std::cout << "ETag:" << etag << "end" << std::endl;

            return size * nmemb;  // 返回实际处理的字节数
        }
        // fwrite(ptr, size, nmemb , stdout);
        return size * nmemb;
    }
    void SetupMutualTLS();
};

}
