#pragma once

#include <chrono>
#include <mutex>
#include <portaudio.h>
#include <complex>
#include <atomic>
#include <functional>
#include <chrono>
#include <cmath>
#include <fmt/format.h>

#include <filesystem>

#include <tsf/tsf.h>
#include <tsf/tml.h>
#include <concurrentqueue/concurrentqueue.h>
#include <zest/common.h>

#include <zest/logger/logger.h>
#include <zest/memory/memory.h>
#include <zest/time/profiler.h>
#include <zest/time/timer.h>
#include <zest/file/file.h>
#include <zest/file/runtree.h>

#include <glm/gtc/constants.hpp>

#include <zest/include/zest/ui/imgui_extras.h>
#include <zest/math/imgui_glm.h>

extern "C" {
#include <soundpipe/h/soundpipe.h>
}

namespace fs = std::filesystem;
