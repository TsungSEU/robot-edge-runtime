#pragma once
#include <string>
#include <vector>

namespace aurora::collector {

class FileCompress{
public:
    enum class ErrorCode {
        Success = 0,
        InvalidInputPath,
        FailedToOpenFile,
        FailedToCreateOutput,
        CompressionFailed,
        UnknownError,
        FailedToCreateTarFile
    };

    enum class CompressionFormat {
        Lz4 = 0,
        Gzip = 1
    };

    FileCompress() = default;
    virtual ~FileCompress() = default;

    static ErrorCode CompressFiles(const std::vector<std::string>& inputFiles,
                                   const std::string& outputFile,
                                   CompressionFormat format = CompressionFormat::Lz4);
    ErrorCode CompressSingleFileToLz4(const std::string& inputFile,
                                      const std::string& outputFile,
                                      CompressionFormat format = CompressionFormat::Lz4);

private:
    static ErrorCode CompressDataLz4(const std::vector<char>& input, std::vector<char>& compressedData);
    static ErrorCode CompressDataGzip(const std::vector<char>& input, std::vector<char>& compressedData);
    static ErrorCode GetFilesInDirectory(const std::string& directory, std::vector<std::string>& fileList);

};

}
