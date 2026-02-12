// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "utils/time.h"

namespace Utils {

HighResTimer::HighResTimer() : running(false) {}

void HighResTimer::start() {
    startTime = std::chrono::high_resolution_clock::now();
    running = true;
}

void HighResTimer::stop() {
    stopTime = std::chrono::high_resolution_clock::now();
    running = false;
}

double HighResTimer::elapsedSeconds() const {
    auto end = running ? std::chrono::high_resolution_clock::now() : stopTime;
    return std::chrono::duration<double>(end - startTime).count();
}

double HighResTimer::elapsedMilliseconds() const {
    auto end = running ? std::chrono::high_resolution_clock::now() : stopTime;
    return std::chrono::duration<double, std::milli>(end - startTime).count();
}

double HighResTimer::elapsedMicroseconds() const {
    auto end = running ? std::chrono::high_resolution_clock::now() : stopTime;
    return std::chrono::duration<double, std::micro>(end - startTime).count();
}

double HighResTimer::elapsedNanoseconds() const {
    auto end = running ? std::chrono::high_resolution_clock::now() : stopTime;
    return std::chrono::duration<double, std::nano>(end - startTime).count();
}

} // namespace Utils
