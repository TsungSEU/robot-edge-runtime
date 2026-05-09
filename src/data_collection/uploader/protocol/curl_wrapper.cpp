#include "curl_wrapper.h"
#include <cstring>
#include "common/log/logger.h"

namespace aurora::collector {

CURLcode CurlWrapper::Init(const std::string& client_cert_path, const std::string& client_key_path, const std::string& ca_cert_path) {
    client_cert_path_ = client_cert_path;//客户端证书路径
    client_key_path_ = client_key_path;//客户端私钥路径
    ca_cert_path_ = ca_cert_path;//根CA证书路径
    curl_global_init(CURL_GLOBAL_ALL);//cURL全局初始化
    curl_ = curl_easy_init();
    if (curl_ == nullptr) {
        AD_ERROR(CurlWrapper, "Failed to initialize Curl.");
        return CURLE_FAILED_INIT;
    }
    return CURLE_OK;
}

CURLcode CurlWrapper::IsInited() const {
    return curl_ != nullptr ? CURLE_OK : CURLE_FAILED_INIT;
}

void CurlWrapper::SetupMutualTLS() {
    if (!client_cert_path_.empty() && !client_key_path_.empty()) {
        // 设置客户端证书和私钥
        curl_easy_setopt(curl_, CURLOPT_SSLCERT, client_cert_path_.c_str());
        curl_easy_setopt(curl_, CURLOPT_SSLKEY, client_key_path_.c_str());

        // std::cout << "client_cert_path_:" << client_cert_path_ << std::endl;
        // std::cout << "client_key_path_:" << client_key_path_ << std::endl;

        // 如果私钥有密码，可以在这里设置
        // curl_easy_setopt(curl_, CURLOPT_KEYPASSWD, "xxx");
    }

    if (!ca_cert_path_.empty()) {
        // 设置CA证书，用于验证服务器证书
        curl_easy_setopt(curl_, CURLOPT_CAINFO, ca_cert_path_.c_str());
        // std::cout << "ca_cert_path_:" << ca_cert_path_ << std::endl;
    }

    // 启用SSL验证（双向认证要求）
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 1L); // 验证服务器证书
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 2L); // 验证主机名

    // 设置SSL版本（可选）
    // curl_easy_setopt(curl_, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
}

CURLcode CurlWrapper::HttpPost(const std::string& url, const std::string& data, std::string& response, const std::vector<std::string>& heads) {
    if (IsInited() != CURLE_OK) {
        AD_ERROR(CurlWrapper, "Curl is not initialized.");
        return CURLE_FAILED_INIT;
    }

    // 清除设置并重置为初始状态
    // curl_easy_reset(curl_);

    CURLcode res;
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    SetupMutualTLS();
    // curl_easy_setopt(curl_, CURLOPT_HTTPGET, false);
    // 关闭自定义请求方法
    curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, nullptr);
    curl_easy_setopt(curl_, CURLOPT_POST, 1L);//设置请求类型为POST类型
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, data.size());
    struct curl_slist* headers = nullptr;
    for (const auto& head : heads) {
        headers = curl_slist_append(headers, head.c_str());
    }
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);

    res = curl_easy_perform(curl_);//执行请求
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        AD_ERROR(CurlWrapper, "Failed to perform HTTP POST request: %s", curl_easy_strerror(res));
    }
    return res;
}

CURLcode CurlWrapper::HttpGet(const std::string& url, std::string& response, const std::vector<std::string>& heads) {
    if (IsInited() != CURLE_OK) {
        AD_ERROR(CurlWrapper, "Curl is not initialized.");
        return CURLE_FAILED_INIT;
    }

    // 清除设置并重置为初始状态
    // curl_easy_reset(curl_);

    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    SetupMutualTLS();
    // 关闭自定义请求方法
    curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, nullptr);
    curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
    struct curl_slist* headers = nullptr;
    for (const auto& head : heads) {
        headers = curl_slist_append(headers, head.c_str());
    }
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl_);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        AD_ERROR(CurlWrapper, "Failed to perform HTTP GET request: %s", curl_easy_strerror(res));
    }
    return res;
}

CURLcode CurlWrapper::HttpPut(const std::string& url, const std::vector<char>& data, std::string& response, const std::vector<std::string>& heads) {
    if (IsInited() != CURLE_OK) {
        AD_ERROR(CurlWrapper, "Curl is not initialized.");
        return CURLE_FAILED_INIT;
    }

    // 清除设置并重置为初始状态
    // curl_easy_reset(curl_);

    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
    // SetupMutualTLS();
    //curl_easy_setopt(curl_, CURLOPT_HTTPGET, 0L);  // 明确设置为非 GET 请求
    curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "PUT");
    struct curl_slist* headers = nullptr;
    for (const auto& head : heads) {
        headers = curl_slist_append(headers, head.c_str());
    }
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.data());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, data.size());
    // 转换为 string 并输出
    std::string str(data.begin(), data.begin() + 10);
    // curl_easy_setopt(curl_, CURLOPT_READFUNCTION, NULL);  // 无需读取函数
    // curl_easy_setopt(curl_, CURLOPT_READDATA, data.data());      // 指定要发送的数据
    // curl_easy_setopt(curl_, CURLOPT_INFILESIZE_LARGE, (curl_off_t)(data.size()));  // 设置数据大小

    // 设置回调函数用于处理响应数据
    // curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback1);
    // curl_easy_setopt(curl_, CURLOPT_WRITEDATA, response_file);

    // 初始化一个字符串来存储ETag值
    char etag[33];
    memset(etag, 0, sizeof(etag));

    // 使用回调函数处理头部信息
    curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, WriteHeadCallback1);
    curl_easy_setopt(curl_, CURLOPT_HEADERDATA, etag);



    // curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
    // curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1);
    // 需要设置CURLOPT_FOLLOWLOCATION，以启用自动重定向功能
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);

    // 设置 stderr 来捕获 SSL 报警信息
    curl_easy_setopt(curl_, CURLOPT_STDERR, stdout);

    CURLcode res = CURLE_FAILED_INIT;

    try {
        res = curl_easy_perform(curl_);
    } catch (const std::exception& e) {
        // 捕获标准异常并打印错误信息
        std::cerr << "Standard exception caught: " << e.what() << std::endl;
    } catch (...) {
        // 捕获所有其他类型的异常
        std::cerr << "Unknown exception caught" << std::endl;
    }


    // CURLcode res = curl_easy_perform(curl_);

    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        AD_ERROR(CurlWrapper, "Failed to perform HTTP PUT request: %s", curl_easy_strerror(res));

        // 打印所有错误代码和消息
        const char *http_err;
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_err);
        if (http_err)
        {
            AD_ERROR(CurlWrapper, "HTTP status code: %s\n", http_err);
        }
    }
    response = etag;

    return res;
}

}
