#pragma once
#include <sys/timeb.h>
#include <string>
using std::string;
class Log
{
private:
    std::string logPosition;
	timeb tb;
public:
    enum LoggingLevel
    {
        INFO,
        WARNING,
        ERROR
    };

public:
    Log(const char *path);
    Log(const string& path);
	void init (const string& path);
    ~Log();
    // void init(const char *path);
    void writeLog(Log::LoggingLevel level, std::string Content);

};