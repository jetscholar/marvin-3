#ifndef LOGGER_H
#define LOGGER_H

#include <cstdarg>
#include <cstdint>

class Logger {
public:
    static void init(int level = 0); // Default argument set to 0 (all logs)
    static void log(int level, const char* format, ...);

private:
    static int debugLevel_;
};

#endif