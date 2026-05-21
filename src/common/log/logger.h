//
// Created by xucong on 25-5-7.
// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
// Tsung Xu<congx0829@163.com>
//

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <vector>
#include <string>

#include "logger_config.h"

/* output log's level */
enum LOG_LEVEL {
  LOG_LEVEL_NONE = -3,
  LOG_LEVEL_ERROR = -2,
  LOG_LEVEL_WARNING = -1,
  LOG_LEVEL_INFO = 0,
  LOG_LEVEL_DEBUG1 = 1,
  LOG_LEVEL_DEBUG2 = 2,
  LOG_LEVEL_DEBUG3 = 3,
  LOG_LEVEL_DEBUG4 = 4,
  LOG_LEVEL_DEBUG5 = 5,
  LOG_LEVEL_DEBUG6 = 6,
  LOG_LEVEL_DEBUG7 = 7,
  LOG_LEVEL_DEBUG8 = 8,
  LOG_LEVEL_DEBUG9 = 9,
};

/**
 * log API short definition
 */
#define AD_INFO(TAG,...)                                                 \
  aurora::common::Logger::instance()->Log(LOG_LEVEL_INFO, #TAG, __FILE__,   \
    __LINE__, __VA_ARGS__)
#define AD_ERROR(TAG, ...)                                               \
  aurora::common::Logger::instance()->Log(LOG_LEVEL_ERROR, #TAG, __FILE__,  \
    __LINE__, __VA_ARGS__)
#define AD_WARN(TAG,...)                                                  \
  aurora::common::Logger::instance()->Log(LOG_LEVEL_WARNING, #TAG, __FILE__, \
    __LINE__, __VA_ARGS__)
#define AD_DEBUG(TAG, ...)                                                \
  aurora::common::Logger::instance()->Log(LOG_LEVEL_DEBUG1, #TAG, __FILE__,  \
    __LINE__, __VA_ARGS__)


namespace aurora::common {

/* the content length and struct of log tag/keyword */
#define LOG_TAG_MAX_LEN 16
typedef struct {
  char tag[LOG_TAG_MAX_LEN + 1];
} LogTag;


#define LOG_KW_MAX_LEN 16
typedef struct {
  char keyword[LOG_KW_MAX_LEN + 1];
} LogKw;

/**
 * output log's filetr
 * level:    output log when Logger's level >= filter's level
 * tags:     if tags' size > 0, output log when Logger's LOG_TAG beyond filter's
 * tags; if tags' size == 0, outpou log normally keywords: if keywords' size >
 * 0, output log when Logger's content contain any of filter's keyword; if
 * keywords' size == 0, output log normally
 */
typedef struct {
  LOG_LEVEL level;
  std::vector<LogTag> tags;
  std::vector<LogKw> keywords;
} LogFilter, *LogFilter_t;

/* log output to console or file */
enum LogOutputMode {
  LOG_TO_CONSOLE = 1 << 0,  // log output to console
  LOG_TO_FILE = 1 << 1,     // log output to file
};

#define LOG_CHECK_AND_RETURN(EXPR)                                       \
  if (!(EXPR)) {                                                         \
    printf("Log is not inited, return\n");                               \
    return;                                                              \
  }

#define CHECK_AND_RETURN(condition, tag, msg, rval)                     \
  do {                                                                  \
    if (!(condition)) {                                                 \
      AD_ERROR(tag, "%s", msg);                                         \
      return rval;                                                      \
    }                                                                   \
  } while (0)

/**
 * @class Logger
 *
 * @brief Light-weight log system implement.
 */
class Logger {
 public:
  /**
   * @brief Singleton mode
   * @return Logger instance
   */
  static Logger* instance();
  static std::once_flag init_flag;

  /**
   * @brief Init func must be executed before the other Logger functions.
   * @param toConsoleFile[In] : log output to console or file
   *        nLevel[In]        : log level
   *        pLogPath[In]      : log file path
   * @return true:success, false:failed
   */
  bool Init(const uint8_t toConsoleFile, int nLevel, const char* const pLogPath, const char* const csvLogPath);

  /**
   * @brief Used with Init together. Init is executed at the begin and Uninit is
   * executed at the end of the program.
   */
  void Uninit();

  /* set log level, tag and keyword */
  void SetLevel(int nLevel);
  void AddTag(const char* const tag);
  void AddKeyword(const char* const keyword);
  void ResetTag();
  void ResetKeyword();

  /* get log level, tag and keyword */
  int GetLevel();
  void GetTags(std::vector<LogTag>& tags);
  void GetKeywords(std::vector<LogKw>& keywords);

  /* rotate the log file */
  bool LogFileRotate();

  /* write strs to the log file */
  void LogToFile(std::string& strs);

  /* write strs to the log csvfile */
  std::vector<std::string> split(std::string str,std::string pattern1,std::string pattern2) ;
  void LogToCsvFile(std::string strs);

  /**
   * @brief output the log
   *
   * @param nLevel[In] level
   * @param tag[In] tag
   * @param pszFile[In] file name
   * @param lineNo[In] line number
   * @param pszFmt[In] output format
   * @param ...[In] args
   */
  void Log(const long& nLevel, const char* const tag, const char* const pszFile, const int& lineNo,
           const char* pszFmt, ...);

  /**
   * @brief output the log
   *
   * @param nLevel[In] level
   * @param tag[In] tag
   * @param pszFmt[In] output format
   * @param ...[In] args
   */
  void Log(const long& nLevel, const char* const tag, const char* pszFmt, ...);

  //save to csv
  void Log(const long& nLevel, const char* const pszFile, const int& lineNo, const char* pszFmt, ...);

 private:
  Logger();
  ~Logger();
  Logger(const Logger& rhs) = delete;
  Logger& operator=(const Logger& rhs) = delete;

 private:
  static Logger* pLog;
  std::mutex m_mutex;
  LogFilter m_logFilter;

  /* log file handle */
  FILE* m_fp;
  std::ifstream inFile;
  std::ofstream csvFile;

  /* log file path */
  char m_logPath[MAX_FILE_PATH_LEN];
  char csv_logPath[MAX_FILE_PATH_LEN];

  /* current log file length */
  uint64_t m_totalLogLen;

  /* output mode: 0=none, 1=console, 2=file, 3=both */
  uint8_t m_outputMode;

  /* initialization flag */
  bool m_initFlag;
};

} 

#endif
