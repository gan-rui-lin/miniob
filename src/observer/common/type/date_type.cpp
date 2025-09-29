/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "common/type/date_type.h"
#include "common/type/char_type.h"
#include "common/time/datetime.h"
#include "common/value.h"
#include "common/log/log.h"
#include "common/lang/comparator.h"
#include "storage/common/column.h"
#include <sstream>
#include <regex>

int DateType::compare(const Value &left, const Value &right) const
{

  ASSERT(left.attr_type() == AttrType::DATES && right.attr_type() == AttrType::DATES, "invalid type");
  if (left.attr_type() != AttrType::DATES || right.attr_type() != AttrType::DATES) {
    LOG_WARN("invalid type to compare. left=%s, right=%s", 
        attr_type_to_string(left.attr_type()), attr_type_to_string(right.attr_type()));
    return INT32_MAX;
  }

  int32_t left_days = *(int32_t *)left.data();
  int32_t right_days = *(int32_t *)right.data();

  if (left_days < right_days) {
    return -1;
  } else if (left_days > right_days) {
    return 1;
  } else {
    return 0;
  }
}

int DateType::compare(const Column &left, const Column &right, int left_idx, int right_idx) const
{
  ASSERT(left.attr_type() == AttrType::DATES, "left type is not date");
  ASSERT(right.attr_type() == AttrType::DATES, "right type is not date");
  
  int32_t left_days = *(int32_t *)&((int32_t*)left.data())[left_idx];
  int32_t right_days = *(int32_t *)&((int32_t*)right.data())[right_idx];
  
  return common::compare_int(&left_days, &right_days);
}

RC DateType::cast_to(const Value &val, AttrType type, Value &result) const
{
  if (val.attr_type() != AttrType::DATES) {
    return RC::INVALID_ARGUMENT;
  }

  switch (type) {
    case AttrType::DATES: {
      result.set_value(val);
      return RC::SUCCESS;
    }
    case AttrType::CHARS: {
      string str_result;
      RC rc = to_string(val, str_result);
      if (rc != RC::SUCCESS) {
        return rc;
      }
      result.set_string(str_result.c_str());
      return RC::SUCCESS;
    }
    default: {
      return RC::UNSUPPORTED;
    }
  }
}

RC DateType::set_value_from_str(Value &val, const string &data) const
{
  int year, month, day;
  sscanf(data.c_str(), "%d-%d-%d", &year, &month, &day);
  int julian_day = common::DateTime::julian_date(year, month, day);
  long long days = julian_day - common::DateTime::JULIAN_19700101;
  val.set_date(days);
  return RC::SUCCESS;
}


bool DateType::is_leap_year(int year)
{
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int DateType::days_in_month(int year, int month)
{
  static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  
  if (month < 1 || month > 12) {
    return 0;
  }
  
  if (month == 2 && is_leap_year(year)) {
    return 29;
  }
  
  return days[month];
}

bool DateType::is_valid_date(int year, int month, int day)
{
  if (year < 1 || month < 1 || month > 12 || day < 1) {
    return false;
  }
  
  int max_days = days_in_month(year, month);
  return day <= max_days;
}

int32_t DateType::date_to_days(int year, int month, int day)
{
  if (!is_valid_date(year, month, day)) {
    LOG_DEBUG("无效日期 %d-%d-%d", year, month, day);
    return INT32_MIN;
  }

  // 使用标准的天数计算算法
  // 这是经过验证的简单可靠算法
  
  // 1970年1月1日为第0天
  const int BASE_YEAR = 1970;
  const int BASE_MONTH = 1;
  const int BASE_DAY = 1;
  
  int total_days = 0;
  
  // 如果是1970年之前的日期，返回负数天数
  if (year < BASE_YEAR || (year == BASE_YEAR && month < BASE_MONTH) || 
      (year == BASE_YEAR && month == BASE_MONTH && day < BASE_DAY)) {
    // 处理1970年之前的日期（向前计算）
    int y = year;
    int m = month;
    int d = day;
    
    // 计算到1970-01-01的天数（负数）
    while (y < BASE_YEAR || (y == BASE_YEAR && m < BASE_MONTH) || 
           (y == BASE_YEAR && m == BASE_MONTH && d < BASE_DAY)) {
      d++;
      if (d > days_in_month(y, m)) {
        d = 1;
        m++;
        if (m > 12) {
          m = 1;
          y++;
        }
      }
      total_days--;
    }
    return total_days;
  }
  
  // 处理1970年及之后的日期
  for (int y = BASE_YEAR; y < year; y++) {
    total_days += is_leap_year(y) ? 366 : 365;
  }
  
  // 计算当年的天数
  static const int month_days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  for (int m = 1; m < month; m++) {
    total_days += month_days[m];
    if (m == 2 && is_leap_year(year)) {
      total_days += 1; // 闰年2月多一天
    }
  }
  
  // 加上当月的天数
  total_days += (day - BASE_DAY);
  
  LOG_DEBUG("日期 %d-%d-%d 转换为 %d 天", year, month, day, total_days);
  
  return (int32_t)total_days;
}

void DateType::days_to_date(int32_t days, int &year, int &month, int &day)
{
  LOG_DEBUG("days_to_date输入: %d天", days);
  
  // 1970年1月1日为基准
  year = 1970;
  month = 1;
  day = 1;
  
  if (days == 0) {
    // 正好是1970-01-01
    LOG_DEBUG("days_to_date转换: %d天 -> %d-%d-%d", days, year, month, day);
    return;
  }
  
  if (days > 0) {
    // 1970年之后的日期
    int32_t remaining_days = days;
    
    // 计算年份
    while (remaining_days > 0) {
      int year_days = is_leap_year(year) ? 366 : 365;
      if (remaining_days >= year_days) {
        remaining_days -= year_days;
        year++;
      } else {
        break;
      }
    }
    
    // 计算月份
    static const int month_days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    month = 1;
    while (month <= 12 && remaining_days > 0) {
      int current_month_days = month_days[month];
      if (month == 2 && is_leap_year(year)) {
        current_month_days = 29;
      }
      
      if (remaining_days >= current_month_days) {
        remaining_days -= current_month_days;
        month++;
      } else {
        break;
      }
    }
    
    // 剩余的就是天数
    day = remaining_days + 1;
  } else {
    // 1970年之前的日期
    int32_t remaining_days = -days;
    
    // 向前回退
    while (remaining_days > 0) {
      day--;
      if (day < 1) {
        month--;
        if (month < 1) {
          year--;
          month = 12;
        }
        day = days_in_month(year, month);
      }
      remaining_days--;
    }
  }
  
  LOG_DEBUG("days_to_date转换: %d天 -> %d-%d-%d", days, year, month, day);
}

RC DateType::to_string(const Value &val, string &result) const
{
  if (val.attr_type() != AttrType::DATES) {
    return RC::INVALID_ARGUMENT;
  }

  int32_t days = *(int32_t *)val.data();
  LOG_DEBUG("to_string读取到天数: %d", days);
  int year, month, day;
  days_to_date(days, year, month, day);
  LOG_DEBUG("days_to_date转换: %d天 -> %d-%d-%d", days, year, month, day);

  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
  result = buffer;
  return RC::SUCCESS;
}



RC DateType::parse_date_string(const string &date_str, int &year, int &month, int &day)
{
  // 匹配 YYYY-MM-DD 格式
  std::regex date_regex(R"(^\s*(\d{4})-(\d{1,2})-(\d{1,2})\s*$)");
  std::smatch match;
  
  if (!std::regex_match(date_str, match, date_regex)) {
    return RC::INVALID_ARGUMENT;
  }
  
  try {
    year = std::stoi(match[1].str());
    month = std::stoi(match[2].str());
    day = std::stoi(match[3].str());
    return RC::SUCCESS;
  } catch (const std::exception &e) {
    return RC::INVALID_ARGUMENT;
  }
}