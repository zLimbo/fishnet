#pragma once

#include "fishnet/base/copyable.h"
#include "fishnet/base/types.h"

struct tm;

namespace fishnet {

class Date : public fishnet::copyable {
public:
    struct YearMonthDay {
        int year;   // [1900..2500]
        int month;  // [1..12]
        int day;    // [1..31]
    };

    static const int kDaysPerWeek = 7;
    static const int kJulianDayOf1970_01_01;

    Date() : julianDayNumber_(0) {}

    Date(int year, int month, int day);

    explicit Date(int julianDayNum) : julianDayNumber_(julianDayNum) {}

    explicit Date(const struct tm&);

    void swap(Date& that) {
        std::swap(julianDayNumber_, that.julianDayNumber_);
    }

    string toIsoString() const;

    YearMonthDay yearMonthDay() const;

    int year() const {
        return yearMonthDay().year;
    }
    int month() const {
        return yearMonthDay().month;
    }
    int day() const {
        return yearMonthDay().day;
    }

    int weekDay() const {
        return (julianDayNumber_ + 1) % kDaysPerWeek;
    }

    int julianDayNumber() const {
        return julianDayNumber_;
    }

private:
    int julianDayNumber_;
};

inline bool operator<(Date x, Date y) {
    return x.julianDayNumber() < y.julianDayNumber();
}

}  // namespace fishnet