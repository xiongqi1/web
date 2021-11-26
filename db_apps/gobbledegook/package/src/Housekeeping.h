#pragma once

//
// Logging
//

// Maximum time to wait for any single async process to timeout during initialization
#define kMaxAsyncInitTimeoutMS (30 * 1000)

enum LogLevel
{
    Debug,
    Verbose,
    Normal,
    ErrorsOnly
};

extern LogLevel logLevel;

// Logger functions defined in standalone, kinda backwards to do this..
void LogDebug(const char *pText);
void LogInfo(const char *pText);
void LogStatus(const char *pText);
void LogWarn(const char *pText);
void LogError(const char *pText);
void LogFatal(const char *pText);
void LogAlways(const char *pText);
void LogTrace(const char *pText);

void signalHandler(int signum);
