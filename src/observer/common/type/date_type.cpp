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
#include "common/value.h"
#include "common/log/log.h"
#include "common/lang/comparator.h"
#include "storage/common/column.h"
#include <sstream>
#include <regex>

int DateType::compare(const Value &left, const Value &right) const
{
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
  RC rc = parse_date_string(data, year, month, day);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  if (!is_valid_date(year, month, day)) {
    return RC::INVALID_ARGUMENT;
  }

  int32_t days = date_to_days(year, month, day);
  LOG_DEBUG("解析日期 %s -> %d-%d-%d -> %d 天", data.c_str(), year, month, day, days);
  if (days == INT32_MIN) {
    return RC::INVALID_ARGUMENT;
  }

  val.set_type(AttrType::DATES);
  val.set_data((char *)&days, sizeof(days));
  return RC::SUCCESS;
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

  // 计算自1970-01-01以来的天数
  // 使用简单可靠的算法
  
  // 1970年1月1日作为基准点（第0天）
  int base_year = 1970;
  int base_day = 1;
  
  int64_t total_days = 0;
  
  // 计算年份差异的天数
  for (int y = base_year; y < year; y++) {
    total_days += is_leap_year(y) ? 366 : 365;
  }
  LOG_DEBUG("年份 %d, 基准年 %d, 年份天数: %lld", year, base_year, (long long)total_days);
  
  // 计算月份差异的天数
  static const int month_days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  for (int m = 1; m < month; m++) {
    total_days += month_days[m];
    if (m == 2 && is_leap_year(year)) {
      total_days += 1; // 闰年2月多一天
    }
  }
  LOG_DEBUG("加上月份天数后: %lld", (long long)total_days);
  
  // 加上天数差异
  total_days += (day - base_day);
  LOG_DEBUG("最终计算结果: %lld 天", (long long)total_days);
  
  // 检查是否溢出
  if (total_days < INT32_MIN || total_days > INT32_MAX) {
    return INT32_MIN;
  }

  return (int32_t)total_days;
}

void DateType::days_to_date(int32_t days, int &year, int &month, int &day)
{
  // 将1970-01-01以来的天数转换为日期
  // 使用与date_to_days相对应的逆算法
  LOG_DEBUG("days_to_date输入: %d天", days);
  
  int64_t remaining_days = days;
  year = 1970;
  
  // 计算年份
  while (remaining_days >= 365) {
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
  while (month <= 12) {
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
  day = (int)remaining_days + 1;
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