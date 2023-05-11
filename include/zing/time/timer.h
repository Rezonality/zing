#pragma once

#include <chrono>
#include <date/date.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <sstream>
#include <mutex>
#include <vector>

namespace NodeGraph
{

// Useful chrono stuff
using TimeSpan = std::chrono::seconds;

// A second timer, based on system clock and therefore UTC epoch correct
using DateTime = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;
using DaysAsSeconds = std::chrono::duration<int, std::ratio<24 * 3600>>;

const auto HalfDaySeconds = std::chrono::seconds(date::days(1)).count() / 2;
const auto FullDaySeconds = std::chrono::seconds(date::days(1)).count();
const int64_t YearDays = date::floor<date::days>(date::years(1)).count();
const int64_t MonthDays = date::floor<date::days>(date::months(1)).count();
const int64_t WeekDays = date::floor<date::days>(date::weeks(1)).count();
const double DaysToYears = 1 / double(YearDays);

DateTime datetime_now();
DateTime datetime_from_seconds(uint64_t t);
DateTime datetime_from_seconds(std::chrono::seconds s);

date::sys_time<std::chrono::milliseconds> sys_time_from_iso_8601(const std::string& str);
DateTime datetime_from_iso_8601(const std::string& str);
std::string datetime_to_iso_8601(DateTime dt);

enum class TimerSample : uint32_t
{
    None,
    Restart
};

template<typename tClock = std::chrono::high_resolution_clock>
struct timerT
{
    int64_t startTime = 0;
    tClock clock;
};

using timer = timerT<>;
using utc_timer = timerT<std::chrono::system_clock>;

template<class T>
uint64_t timer_get_time_now(timerT<T>& timer)
{
    // NOTE! This is using high resolution clock and is not relative to UTC or any other calendar!
    // https://stackoverflow.com/questions/26128035/c11-how-to-print-out-high-resolution-clock-time-point
    // If you want a real-world clock, it won't do
    return std::chrono::duration_cast<std::chrono::nanoseconds>(timer.clock.now().time_since_epoch()).count();
}

template<class T>
void timer_start(timerT<T>& timer)
{
    timer_restart(timer);
}

template<class T>
void timer_set_earliest(timerT<T>& timer)
{
    timer.startTime = 0;
}

template<class T>
void timer_restart(timerT<T>& timer)
{
    timer.startTime = std::chrono::duration_cast<std::chrono::nanoseconds>(timer.clock.now().time_since_epoch()).count();
}

template<class T>
uint64_t timer_get_elapsed(const timerT<T>& timer)
{
    auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(timer.clock.now().time_since_epoch()).count();
    return now - timer.startTime;
}

template<class T>
uint64_t timer_to_epoch_utc_seconds(const timerT<T>& timer)
{
    auto nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(timer.clock.now().time_since_epoch());
    auto utcNow = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
    auto diff = utcNow - nowSeconds;

    return (timer.startTime / 1000000000) + diff.count();
}

double timer_to_seconds(uint64_t value);
double timer_to_ms(uint64_t value);

template<class T>
double timer_get_elapsed_seconds(const timerT<T>& timer)
{
    return timer_to_seconds(timer_get_elapsed(timer));
}


struct TimeRange
{
    DateTime Start = DateTime::min();
    DateTime End = DateTime::min();
    bool Valid() const
    {
        return Start != DateTime::min() && End != DateTime::min();
    }
    TimeSpan Duration() const
    {
        return TimeSpan(End.time_since_epoch() - Start.time_since_epoch());
    }
    bool InRange(const DateTime& val) const
    {
        return val.time_since_epoch() >= Start.time_since_epoch() && val.time_since_epoch() <= End.time_since_epoch();
    }
    void IncludeRange(DateTime time)
    {
        Start = std::min(time, Start);
        End = std::max(time, End);
    }
    void IncludeRange(DateTime start, DateTime end)
    {
        Start = std::min(start, Start);
        Start = std::max(start, Start);
        End = std::min(end, End);
        End = std::max(end, End);
    }
   
    /*
    std::string to_string() const
    {
        using namespace date;
        std::ostringstream str;
        str << Start << " - " << End;
        return str.str();
    }
    */
};

enum class DateTimeFormat
{
    DayMonth,
    JsonDayMonthYear,
    YearMonthDay,
    YearMonthDayTime,
    Year,
    Month,
    Day,
    Time,
    Value
};

std::string timerange_to_string(TimeRange r, DateTimeFormat format);
std::string datetime_to_string(DateTime d, DateTimeFormat format);

} // namespace NodeGraph
