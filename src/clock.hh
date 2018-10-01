#pragma once

#include <chrono>

namespace Kakoune
{

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

}
