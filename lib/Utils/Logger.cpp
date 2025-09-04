#include "Logger.h"
#include <cstdio>

int Logger::debugLevel_ = 0;

void Logger::init(int level) {
    debugLevel_ = level;
}

void Logger::log(int level, const char* format, ...) {
    if (level >= debugLevel_) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
    }
}