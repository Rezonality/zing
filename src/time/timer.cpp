#include <chrono>
#include <sstream>
#include <string>

#include <zing/logger/logger.h>
#include <zing/time/timer.h>

using namespace std::chrono;
using namespace date;

namespace Zing
{

timer globalTimer;

double timer_to_seconds(uint64_t value)
{
    return double(value / 1000000000.0);
}

double timer_to_ms(uint64_t value)
{
    return double(value / 1000000.0);
}

date::sys_time<std::chrono::milliseconds> sys_time_from_iso_8601(const std::string& str)
{
    std::istringstream in{ str };
    date::sys_time<std::chrono::milliseconds> tp;
    in >> date::parse("%FT%TZ", tp);
    if (in.fail())
    {
        in.clear();
        in.exceptions(std::ios::failbit);
        in.str(str);
        in >> date::parse("%FT%T%Ez", tp);
    }
    return tp;
}

DateTime datetime_from_iso_8601(const std::string& str)
{
    auto t = sys_time_from_iso_8601(str);
    return DateTime(duration_cast<seconds>(t.time_since_epoch()));
}
/*
std::string datetime_to_iso_8601_string(DateTime dt)
{
    return date::format("%y_%m_%d_%H_%M_%S", dt);
}
*/

std::string datetime_to_iso_8601(DateTime tp)
{
    return date::format("%FT%TZ", date::floor<seconds>(tp));
}

DateTime datetime_now()
{
    return DateTime(date::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
}

DateTime datetime_from_seconds(uint64_t t)
{
    return DateTime(std::chrono::seconds(t));
}

DateTime datetime_from_seconds(std::chrono::seconds s)
{
    return DateTime(s);
}

DateTime datetime_from_timer_start(timer& timer)
{
    return DateTime(seconds(timer_to_epoch_utc_seconds(timer)));
}

// Convert DateTime to string 
std::string datetime_to_string(DateTime d, DateTimeFormat format)
{
    auto dp = date::floor<date::days>(d);
    auto ymd = date::year_month_day{ dp };
    auto tm = make_time(d - dp);
    std::ostringstream str;
    switch (format)
    {
    default:
    case DateTimeFormat::JsonDayMonthYear:
        str << ymd.day() << "/" << ymd.month() << "/" << ymd.year();
        return str.str();
    case DateTimeFormat::YearMonthDay:
        str << ymd.day() << " " << ymd.month() << " " << ymd.year();
        return str.str();
    case DateTimeFormat::YearMonthDayTime:
        str << ymd.day() << " " << ymd.month() << " " << ymd.year() << " - " << tm;
        return str.str();
    case DateTimeFormat::DayMonth:
        str << ymd.day() << " " << ymd.month();
        return str.str();
    case DateTimeFormat::Month:
        str << ymd.month();
        return str.str();
    case DateTimeFormat::Year:
        str << ymd.year();
        return str.str();
    case DateTimeFormat::Day:
        str << ymd.day();
        return str.str();
    case DateTimeFormat::Time:
        str << tm;
        return str.str();
    case DateTimeFormat::Value:
        str << d.time_since_epoch().count();
        return str.str();
    }
}

} // namespace Zing
