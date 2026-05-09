#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>

namespace aurora::common {
/* Logger software version number */
static const char* log_sw_version = "log version no is r25.0";
/* String to identify log entries originating from this file */
// static const char* LOG_TAG = "clog";

#ifdef LOG_COLOR_ENABLE
/* color output info */
#define LOG_COLOR_NUM 4
static const char* color_output_info[] = {LOG_COLOR_ERROR, LOG_COLOR_WARN,
                                          LOG_COLOR_INFO, LOG_COLOR_DEBUG};
#endif

std::once_flag Logger::init_flag;
Logger* Logger::pLog = nullptr;

/* Singleton instance  */
Logger* Logger::instance() {
  std::call_once(init_flag, [] { pLog = new Logger; });
  return pLog;
}

Logger::Logger() {
  m_totalLogLen = 0;
  memset(m_logPath, 0, MAX_FILE_PATH_LEN);
  memset(csv_logPath, 0, MAX_FILE_PATH_LEN);
  m_initFlag = false;
  m_fp = nullptr;
}

Logger::~Logger() {}

bool Logger::Init(const uint8_t toConsoleFile, int nLevel, const char* const pLogPath, const char* const csvLogPath) {
  std::unique_lock<std::mutex> lock(m_mutex);

  if (m_initFlag) {
    printf("Logger has already inited\n");
    return true;
  }

  /* bit 0: log to console, bit 1: log to file */
  m_outputMode = toConsoleFile & (LOG_TO_CONSOLE | LOG_TO_FILE);

  /* check nLevel, [LOG_LEVEL_NONE, LOG_LEVEL_DEBUG9] */
  if (nLevel < LOG_LEVEL_NONE)
    nLevel = LOG_LEVEL_NONE;
  else if (nLevel > LOG_LEVEL_DEBUG9)
    nLevel = LOG_LEVEL_DEBUG9;

  m_logFilter.level = (LOG_LEVEL)nLevel;
  m_logFilter.tags.clear();
  m_logFilter.keywords.clear();

  if (toConsoleFile & LOG_TO_FILE) {
    m_fp = fopen(pLogPath, "a");  // if file not existed, create it; add content to the end of the file

    if (!m_fp) {
      printf("Error: fail to open file %s\n", pLogPath);    
      return false;
    }
  }

  if (pLogPath)
    memcpy(m_logPath, pLogPath, strlen(pLogPath) + 1);
  if (csvLogPath)
    memcpy(csv_logPath, csvLogPath, strlen(csvLogPath) + 1);
  m_initFlag = true;

  lock.unlock();

  /* package process info */
  char szProcessID[MAX_THREAD_ID_LEN] = {0};
  snprintf(szProcessID, MAX_THREAD_ID_LEN, "%0d", getpid());
  // AD_INFO(Logger, "Logger is initialize success, process id: %s, info: %s.",
  //          szProcessID, log_sw_version);
  return true;
}

void Logger::Uninit() {
  // Scope the lock to release mutex before object deletion
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initFlag = false;
    if (m_fp) {
      fclose(m_fp);
      m_fp = nullptr;
    }
    if (csvFile.is_open()) {
      csvFile.close();
    }
    if (inFile.is_open()) {
      inFile.close();
    }
  }  // lock_guard destructs here, unlocking m_mutex

  // Safe to delete object now
  if (pLog) {
    delete pLog;
    pLog = nullptr;
    printf("Destroy Logger\n");
  }
}

void Logger::SetLevel(int nLevel) {
    LOG_CHECK_AND_RETURN(m_initFlag);
  std::lock_guard<std::mutex> lock(m_mutex);

  /* check nLevel, [LOG_LEVEL_NONE, LOG_LEVEL_DEBUG9] */
  if (nLevel < LOG_LEVEL_NONE)
    nLevel = LOG_LEVEL_NONE;
  else if (nLevel > LOG_LEVEL_DEBUG9)
    nLevel = LOG_LEVEL_DEBUG9;
  m_logFilter.level = (LOG_LEVEL)nLevel;
}

void Logger::AddTag(const char* const tag) {
    LOG_CHECK_AND_RETURN(m_initFlag);
  std::lock_guard<std::mutex> lock(m_mutex);

  LogTag log_tag;
  memset(log_tag.tag, 0, LOG_TAG_MAX_LEN + 1);
  /* if the tag's len > LOG_TAG_MAX_LEN, truncate it */
  strncpy(log_tag.tag, tag, LOG_TAG_MAX_LEN);
  m_logFilter.tags.push_back(log_tag);
}

void Logger::AddKeyword(const char* const keyword) {
    LOG_CHECK_AND_RETURN(m_initFlag);
  std::lock_guard<std::mutex> lock(m_mutex);
  LogKw log_kw;
  memset(log_kw.keyword, 0, LOG_KW_MAX_LEN + 1);
  /* if the keyword's len > LOG_KW_MAX_LEN, truncate it */
  strncpy(log_kw.keyword, keyword, LOG_KW_MAX_LEN);
  m_logFilter.keywords.push_back(log_kw);
}

void Logger::ResetTag() {
    LOG_CHECK_AND_RETURN(m_initFlag);
  std::lock_guard<std::mutex> lock(m_mutex);
  /* clear the filter tags */
  m_logFilter.tags.clear();
}

void Logger::ResetKeyword() {
    LOG_CHECK_AND_RETURN(m_initFlag);
  std::lock_guard<std::mutex> lock(m_mutex);
  /* clear the filter keywords */
  m_logFilter.keywords.clear();
}

/**
 * @brief get the current log level.
 *
 * @return Logger level
 */
int Logger::GetLevel() {
  std::lock_guard<std::mutex> lock(m_mutex);
  return (int)m_logFilter.level;
}

/**
 * @brief get the current log tags.
 *
 * @param tags[Out] Logger filter tags
 */
void Logger::GetTags(std::vector<LogTag>& tags) {
    LOG_CHECK_AND_RETURN(m_initFlag);
  std::lock_guard<std::mutex> lock(m_mutex);
  tags.clear();
  tags.assign(m_logFilter.tags.begin(), m_logFilter.tags.end());
}

/**
 * @brief get the current log tags.
 *
 * @param tags[Out] Logger filter keywords
 */
void Logger::GetKeywords(std::vector<LogKw>& keywords) {
  LOG_CHECK_AND_RETURN(m_initFlag);
  std::lock_guard<std::mutex> lock(m_mutex);
  keywords.clear();
  keywords.assign(m_logFilter.keywords.begin(), m_logFilter.keywords.end());
}

#ifdef _WIN32
struct tm* localtime_r(const time_t* timeep, struct tm* result) {
  localtime_s(result, timeep);
  return result;
}
#endif

#define LOG_TIME_BUF_SIZE 64
static inline void log_time(char* const buf) {
  time_t t;
  tm local;
  t = time(NULL);
  localtime_r(&t, &local);

  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  int64_t ms = tp.tv_nsec / 1000000;

  snprintf(buf, LOG_TIME_BUF_SIZE - 1, "%04d%02d%02d %02d:%02d:%02d.%03ld",
           local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour,
           local.tm_min, local.tm_sec, ms);
}

static inline void log_time_csv(char* const buf) {
  time_t t;
  tm local;
  t = time(NULL);
  localtime_r(&t, &local);
  snprintf(buf, LOG_TIME_BUF_SIZE - 1, "%02d-%02d-%02d",local.tm_hour, local.tm_min, local.tm_sec);
}

/**
 * @brief get file name without path.
 * @param filepath[In] file path, eg: /home/kaka/main.cpp
 * @return file name without path, eg: main.cpp
 */
static const char* const_basename(const char* const filepath) {
  const char* base = strrchr(filepath, '/');
  return base ? (base + 1) : filepath;
}

/**
 * @brief judge whether tag beyonds to vec_tag.
 * @param tag[In] Logger tag
 * @param vec_tag[In] Logger filter tags
 * @return true: if vec_tag's size == 0; or tag beyonds to vec_tag if vec_tag's
 * size > 0 false: vec_tag's size > 0 and tag is not beyonded to vec_tag
 */
static bool IsTagInFilter(const char* const tag, std::vector<LogTag>& vec_tag) {
  if (0 == vec_tag.size()) return true;

  for (size_t i = 0; i < vec_tag.size(); i++) {
    if (strcmp(tag, vec_tag[i].tag) == 0) return true;
  }
  return false;
}


/**
 * @brief judge whether log info contains one of vec_keyword.
 * @param info[In] Logger content
 * @param vec_keyword[In] Logger filter keywords
 * @return true: if vec_keyword's size == 0; or info contains one of vec_keyword
 * if vec_keyword's size > 0 false: vec_keyword's size > 0 and info doesn't
 * contain any of vec_keyword
 */
static bool IsKeyInInfo(const char* const info, std::vector<LogKw>& vec_keyword) {
  if (0 == vec_keyword.size()) return true;

  for (size_t i = 0; i < vec_keyword.size(); i++) {
    if (strstr(info, vec_keyword[i].keyword)) return true;
  }
  return false;
}

/*
 * rotate the log file xxx.log.n-2 => xxx.log.n-1, ..., xxx.log => xxx.log.1
 */
bool Logger::LogFileRotate()
{
#define SUFFIX_LEN                     10

  int n, err = 0;
  char oldpath[MAX_FILE_PATH_LEN]= { 0 }, newpath[MAX_FILE_PATH_LEN] = { 0 };
  size_t base = strlen(m_logPath);

  bool result = true;
  FILE *tmp_fp;

  memcpy(oldpath, m_logPath, base);
  memcpy(newpath, m_logPath, base);

  fclose(m_fp);

  for (n = LOG_FILE_MAX_ROTATE - 1; n >= 1; --n) {
    snprintf(oldpath + base, SUFFIX_LEN, n - 1 ? ".%d" : "", n - 1);
    snprintf(newpath + base, SUFFIX_LEN, ".%d", n);

    /* remove the old file */
    if ((tmp_fp = fopen(newpath , "r")) != NULL) {
      fclose(tmp_fp);
      remove(newpath);
    }
    /* change the new log file to old file name */
    if ((tmp_fp = fopen(oldpath , "r")) != NULL) {
      fclose(tmp_fp);
      err = rename(oldpath, newpath);
    }

    if (err < 0) {
      result = false;
      break;
    }
  }
  /* reopen the file */
  m_fp = fopen(m_logPath, "a+");

  return result;
}

/**
 * @brief select color index by log level
 * @param level[In] Logger level
 * @return color index
 */
static uint8_t Level2ColorIndex(const long& level)
{
  switch (level) {
    case LOG_LEVEL_ERROR:
      return 0;
    case LOG_LEVEL_WARNING:
      return 1;
    case LOG_LEVEL_INFO:
      return 2;
    case LOG_LEVEL_DEBUG1:
    case LOG_LEVEL_DEBUG2:
    case LOG_LEVEL_DEBUG3:
    case LOG_LEVEL_DEBUG4:
    case LOG_LEVEL_DEBUG5:
    case LOG_LEVEL_DEBUG6:
    case LOG_LEVEL_DEBUG7:
    case LOG_LEVEL_DEBUG8:
    case LOG_LEVEL_DEBUG9:
      return 3;
    default:
      return 2;
  }
}

/**
 * @brief Write str to the log file
 * @param strs[In] log content
 */
void Logger::LogToFile(std::string& strs)
{
  if (m_outputMode & LOG_TO_FILE) {
    if (m_fp == NULL) return;

#ifdef _WIN32
    strs += "\r\n";
#else
    strs += "\n";
#endif

    m_totalLogLen += strs.length();
    if (m_totalLogLen > LOG_FILE_MAX_SIZE) {
      m_totalLogLen = 0;
      if (false ==LogFileRotate())
        return;
    }
    fwrite(strs.c_str(), 1, strs.length(), m_fp);
    fflush(m_fp);
  }
}

/**
 * @brief Write str to the log csvfile
 * @param strs[In] log content
 */
void Logger::LogToCsvFile( std::string str) {
  std::vector<std::string> result=Logger::split(str,":",",");
  std::string line;
  int result_size = result.size();
  inFile.open(csv_logPath, std::ios::in);
  if (inFile.is_open()) {
    inFile.close();
    csvFile.open(csv_logPath, std::ios::app);
    int i =1;
    for(; i < result_size-2; i+=2)
      csvFile << result[i] << ',';
    csvFile << result[i];
    csvFile << '\n';
  }
  else {
    csvFile.open(csv_logPath, std::ios::out);
    int i = 0;
    for (;i < result_size-2;i+=2)
      csvFile << result[i] << ',';
    csvFile << result[i] << '\n';
    i =1;
    for(; i < result_size-2; i+=2)
      csvFile << result[i] << ',';
    csvFile << result[i];
    csvFile << '\n';
  }
  csvFile.close();
}

std::vector<std::string> Logger::split(std::string str,std::string pattern1,std::string pattern2) {
  std::vector<std::string> result;
  str+=pattern1;
  const std::string::size_type size = str.size();

  for(std::string::size_type i = 0; i < size; i++) {
    std::string::size_type pos1 = str.find(pattern1, i);
    std::string::size_type pos2 = str.find(pattern2, i);
    std::string::size_type pos = pos1 < pos2 ? pos1 : pos2;
    if(pos < size) {
      std::string s = str.substr(i, pos - i);
      result.push_back(s);
      i = pos + pattern1.size() - 1;
    }
  }
  return result;
}

void Logger::Log(const long& nLevel, const char* const tag, const char* const pszFile, const int& lineNo,
                 const char* pszFmt, ...) {
  LOG_CHECK_AND_RETURN(m_initFlag);

  if (((m_outputMode & LOG_TO_CONSOLE) == 0) && ((m_outputMode & LOG_TO_FILE) == 0))
    return;

  /* if tag's length > LOG_TAG_MAX_LEN, truncate it */
  char new_tag[LOG_TAG_MAX_LEN + 1] = { 0 };
  size_t tag_len = strlen(tag);
  for (size_t i = 0; i < LOG_TAG_MAX_LEN && i < tag_len; i++) {
    new_tag[i] = tag[i];
  }

  if (nLevel > m_logFilter.level)
    return;
  else if (0 == IsTagInFilter(new_tag, m_logFilter.tags))
    return;

  std::string strDebugInfo = "";
#ifdef LOG_COLOR_ENABLE
  std::string strColorStart = "";
  /* add CSI start sign and color info */
  strColorStart += CSI_START;
  uint8_t color_idx = Level2ColorIndex(nLevel);
  strColorStart += color_output_info[color_idx];
#endif

  /* package level info */
  if (nLevel == LOG_LEVEL_WARNING)
    strDebugInfo += "[WARN] ";
  else if (nLevel == LOG_LEVEL_ERROR)
    strDebugInfo += "[ERROR] ";
  else if (nLevel == LOG_LEVEL_INFO)
    strDebugInfo += "[INFO] ";
  else
    strDebugInfo += "[DEBUG] ";

#ifdef LOG_TAG_OUTPUT_ENABLE
  strDebugInfo += "[";
    strDebugInfo += new_tag;
    strDebugInfo += "]";
#endif

  /* package time info */
  char szTime[LOG_TIME_BUF_SIZE] = {0};
  log_time(szTime);
  strDebugInfo += szTime;

  /* package thread info */
  char szThreadID[MAX_THREAD_ID_LEN] = {0};
  pid_t curThreadID;
#ifdef _WIN32
  curThreadID = GetCurrentThreadId();
#else
  curThreadID = syscall(SYS_gettid);
#endif
  sprintf(szThreadID, " %d ", curThreadID);
  strDebugInfo += szThreadID;

  /* file name and line number info */
  char szFileLine[MAX_FILE_PATH_LEN];
  sprintf(szFileLine, "[%s:%d] ", const_basename(pszFile), lineNo);
  strDebugInfo += szFileLine;

  /* package other log data */
  va_list ap;
  va_start(ap, pszFmt);
  char szMsg[MAX_LOG_LEN];
  vsnprintf(szMsg, MAX_LOG_LEN, pszFmt, ap);
  va_end(ap);

  strDebugInfo += szMsg;

  if (strDebugInfo.length() > MAX_LOG_LEN)
    strDebugInfo = strDebugInfo.substr(0, MAX_LOG_LEN);

  if (IsKeyInInfo(strDebugInfo.c_str(), m_logFilter.keywords) == 0) return;

#ifdef LOG_COLOR_ENABLE
  /* add CSI end sign */
  std::string strColorEnd = "";
  strColorEnd += CSI_END;
#endif

  /* check if log output to console*/
  if (m_outputMode & LOG_TO_CONSOLE) {
#ifdef LOG_COLOR_ENABLE

#ifdef _WIN32
    printf("%s\r\n", (strColorStart + strDebugInfo + strColorEnd).c_str());
#else
    printf("%s\n", (strColorStart + strDebugInfo + strColorEnd).c_str());
#endif

#else

    #ifdef _WIN32
      printf("%s\r\n", strDebugInfo.c_str());
    #else
      printf("%s\n", strDebugInfo.c_str());
    #endif

#endif
  }

  /* check if log output to file*/
  LogToFile(strDebugInfo);
}

void Logger::Log(const long& nLevel, const char* const tag, const char* pszFmt, ...) {
  LOG_CHECK_AND_RETURN(m_initFlag);
  if (((m_outputMode & LOG_TO_CONSOLE) == 0) && ((m_outputMode & LOG_TO_FILE) == 0))
    return;

  /* if tag's length > LOG_TAG_MAX_LEN, truncate it */
  char new_tag[LOG_TAG_MAX_LEN + 1] = {0};
  size_t tag_len = strlen(tag);
  for (size_t i = 0; i < LOG_TAG_MAX_LEN && i < tag_len; i++) {
    new_tag[i] = tag[i];
  }

  if (nLevel > m_logFilter.level)
    return;
  if (0 == IsTagInFilter(new_tag, m_logFilter.tags))
    return;

  std::string strDebugInfo = "";
#ifdef LOG_COLOR_ENABLE
  /* add CSI start sign and color info */
  std::string strColorStart = "";
  strColorStart += CSI_START;
  uint8_t color_idx = Level2ColorIndex(nLevel);
  strColorStart += color_output_info[color_idx];
#endif

  /* package other log data */
  va_list ap;
  va_start(ap, pszFmt);
  char szMsg[MAX_LOG_LEN];
  vsnprintf(szMsg, MAX_LOG_LEN, pszFmt, ap);
  va_end(ap);

  strDebugInfo += szMsg;

  /* check log length */
  if (strDebugInfo.length() > MAX_LOG_LEN)
    strDebugInfo = strDebugInfo.substr(0, MAX_LOG_LEN);

  /* keyword filter */
  if (IsKeyInInfo(strDebugInfo.c_str(), m_logFilter.keywords) == 0) return;
#ifdef LOG_COLOR_ENABLE
  /* add CSI end sign */
  std::string strColorEnd;
  strColorEnd += CSI_END;
#endif

  /* check if log output to console*/
  if (m_outputMode & LOG_TO_CONSOLE) {
#ifdef LOG_COLOR_ENABLE
    printf("%s", (strColorStart + strDebugInfo + strColorEnd).c_str());    // PRQA S 6001 # thread safe
#else
    printf("%s", strDebugInfo.c_str());    // PRQA S 6001 # thread safe
#endif
  }

  /* check if log output to file*/
  LogToFile(strDebugInfo);
}

void Logger::Log(const long& nLevel, const char* const pszFile, const int& lineNo, const char* pszFmt, ...) {
    LOG_CHECK_AND_RETURN(m_initFlag);

  std::string strCsvInfo = "";

  /* file name and line number info */
  char szFileLine[MAX_FILE_PATH_LEN];
  sprintf(szFileLine, "File Name:%s,Line:%d", const_basename(pszFile), lineNo);
  strCsvInfo += szFileLine;
  strCsvInfo.append(",");

  /* package time info */
  char szTime[LOG_TIME_BUF_SIZE] = {0};
  log_time_csv(szTime);
  strCsvInfo += "Time:";
  strCsvInfo += szTime;
  strCsvInfo.append(",");

  /* package other log data */
  va_list ap;
  va_start(ap, pszFmt);
  char szMsg[MAX_LOG_LEN];
  vsnprintf(szMsg, MAX_LOG_LEN, pszFmt, ap);
  va_end(ap);

  strCsvInfo += szMsg;
  std::cout << "Log Test:" << strCsvInfo <<std::endl;
  /* check if log output to file*/
  LogToCsvFile(strCsvInfo);
}
}  // namespace aurora::logger