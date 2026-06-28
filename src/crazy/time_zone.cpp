#include "crazy/time_zone.h"

#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#endif

namespace crazy {

    TimeZone::TimeZone() {
        offset_ = calculateSystemOffset();
        name_ = getSystemTimeZoneName();
    }
    TimeZone::TimeZone(int32_t offset, const std::string& name)
        : name_(name), offset_(offset) {
        if (name_.empty()) {
            if (offset == 0) {
                name_ = "UTC";
            }
            else {
                char buffer[32];
                int hours = offset / 3600;
                int minutes = (offset % 3600) / 60;

                if (minutes == 0) {
                    snprintf(buffer, sizeof(buffer), "UTC%+d", hours);
                }
                else {
                    snprintf(buffer, sizeof(buffer), "UTC%+d:%02d", hours, minutes);
                }
                name_ = buffer;
            }
        }
    }
    TimeZone TimeZone::UTC() {
        return TimeZone(0, "UTC");
    }
    TimeZone TimeZone::Local() {
        return TimeZone();
    }
    const std::string& TimeZone::getName() const {
        return name_;
    }
    int32_t TimeZone::getOffset() const {
        return offset_;
    }
    void TimeZone::setName(const std::string& name) {
        name_ = name;
    }
    void TimeZone::setOffset(int32_t offset) {
        offset_ = offset;
    }
    time_t TimeZone::toUTC(time_t local_time) const {
        return local_time - offset_;
    }
    time_t TimeZone::toLocal(time_t utc_time) const {
        return utc_time + offset_;
    }
    bool TimeZone::isValid() const {
        return offset_ >= -12 * 3600 && offset_ <= 14 * 3600;
    }
    bool TimeZone::operator==(const TimeZone& other) const {
        return offset_ == other.offset_ && name_ == other.name_;
    }
    bool TimeZone::operator!=(const TimeZone& other) const {
        return !(*this == other);
    }
    int32_t TimeZone::calculateSystemOffset() {
        time_t now = time(nullptr);
        struct tm local_tm;
#ifdef _WIN32
        localtime_s(&local_tm, &now);
#else
        localtime_r(&now, &local_tm);
#endif
        struct tm utc_tm;
#ifdef _WIN32
        gmtime_s(&utc_tm, &now);
#else
        gmtime_r(&now, &utc_tm);
#endif
        int32_t offset = (local_tm.tm_hour - utc_tm.tm_hour) * 3600 +
            (local_tm.tm_min - utc_tm.tm_min) * 60 +
            (local_tm.tm_sec - utc_tm.tm_sec);

        if (offset > 12 * 3600) offset -= 24 * 3600;
        if (offset < -12 * 3600) offset += 24 * 3600;

        return offset;
    }

    std::string TimeZone::getSystemTimeZoneName() {
#ifdef _WIN32
        TIME_ZONE_INFORMATION tz_info;
        if (GetTimeZoneInformation(&tz_info) != TIME_ZONE_ID_INVALID) {
            wchar_t* wname = tz_info.StandardName;
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, wname, -1, NULL, 0, NULL, NULL);
            std::string name(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, wname, -1, &name[0], size_needed, NULL, NULL);
            if (!name.empty() && name.back() == '\0') {
                name.pop_back();
            }
            return name;
        }
        return "Local";
#else
        tzset();
        const char* tz_env = getenv("TZ");
        if (tz_env && tz_env[0] != '\0') {
            return tz_env;
        }

        FILE* fp = fopen("/etc/timezone", "r");
        if (fp) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), fp)) {
                buffer[strcspn(buffer, "\n")] = '\0';
                fclose(fp);
                return buffer;
            }
            fclose(fp);
        }

        char link_path[1024];
        ssize_t len = readlink("/etc/localtime", link_path, sizeof(link_path) - 1);
        if (len > 0) {
            link_path[len] = '\0';

            const char* zoneinfo_prefix = "zoneinfo/";
            char* zoneinfo = strstr(link_path, zoneinfo_prefix);
            if (zoneinfo) {
                return zoneinfo + strlen(zoneinfo_prefix);
            }

            const char* posix_prefix = "posix/";
            char* posix = strstr(link_path, posix_prefix);
            if (posix) {
                return posix + strlen(posix_prefix);
            }

            return link_path;
        }

        if (tzname[0] && tzname[0][0] != '\0') {
            return tzname[0];
        }

        return "Local";
#endif
    }

} // namespace crazy
