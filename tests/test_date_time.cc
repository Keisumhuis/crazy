#include "crazy.h"

int main() {

    // 创建当前时间（1970-01-01 00:00:00）
    crazy::DateTime dt1;
    std::cout << "默认时间: " << dt1.toString() << std::endl;

    // 从时间戳创建
    crazy::DateTime dt2(1702080000);  // 2023-12-09 00:00:00
    std::cout << "时间戳创建: " << dt2.toString() << std::endl;

    // 从各部分创建
    crazy::DateTime dt3(2023, 12, 25, 14, 30, 0);
    std::cout << "圣诞日: " << dt3.toString() << std::endl;

    // 获取各部分
    std::cout << "年份: " << dt3.year() << std::endl;
    std::cout << "月份: " << dt3.month() << std::endl;
    std::cout << "日: " << dt3.day() << std::endl;
    std::cout << "星期: " << static_cast<int>(dt3.weekDay()) << std::endl;
    std::cout << "一年中的第几天: " << dt3.yearDay() << std::endl;

    // 时间运算
    crazy::DateTime tomorrow = dt3;
    tomorrow.addDays(1);
    std::cout << "明天: " << tomorrow.toString() << std::endl;

    crazy::DateTime next_month = dt3;
    next_month.addMonths(1);
    std::cout << "下个月: " << next_month.toString() << std::endl;

    // 格式化输出
    std::cout << "自定义格式: " << dt3.toString("%Y年%m月%d日 %H时%M分%S秒") << std::endl;
    std::cout << "紧凑格式: " << dt3.toString("%Y%m%d%H%M%S") << std::endl;

    // 闰年判断
    crazy::DateTime leap_year(2024, 2, 29);
    std::cout << "2024年是闰年吗? " << (leap_year.isLeapYear() ? "是" : "否") << std::endl;
    std::cout << "2024年2月有 " << leap_year.daysInMonth() << " 天" << std::endl;

    // 比较运算符
    crazy::DateTime dt4(2023, 12, 24);
    crazy::DateTime dt5(2023, 12, 25);
    std::cout << "dt4 < dt5? " << (dt4 < dt5) << std::endl;

    auto dateTime = crazy::DateTime::fromString("2026-12-09 12:21:12");

    return 0;
}
