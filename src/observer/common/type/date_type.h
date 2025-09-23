/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include "common/type/data_type.h"

/**
 * @brief 日期类型
 * @ingroup DataType
 * @details 日期类型，存储格式为 YYYY-MM-DD，内部用 int32_t 存储自1970-01-01以来的天数
 */
class DateType : public DataType
{
public:
  DateType() : DataType(AttrType::DATES) {}
  virtual ~DateType() {}

  int compare(const Value &left, const Value &right) const override;
  int compare(const Column &left, const Column &right, int left_idx, int right_idx) const override;

  RC cast_to(const Value &val, AttrType type, Value &result) const override;

  int cast_cost(const AttrType type) override
  {
    if (type == AttrType::DATES) {
      return 0;
    } else if (type == AttrType::CHARS) {
      return 2;
    }
    return INT32_MAX;
  }

  RC set_value_from_str(Value &val, const string &data) const override;

  RC to_string(const Value &val, string &result) const override;

public:
  /**
   * @brief 检查日期是否合法
   * @param year 年份
   * @param month 月份 (1-12)
   * @param day 天数 (1-31)
   * @return true 如果日期合法，false 否则
   */
  static bool is_valid_date(int year, int month, int day);

  /**
   * @brief 检查是否为闰年
   * @param year 年份
   * @return true 如果是闰年，false 否则
   */
  static bool is_leap_year(int year);

  /**
   * @brief 获取指定月份的天数
   * @param year 年份
   * @param month 月份 (1-12)
   * @return 该月的天数，错误时返回 0
   */
  static int days_in_month(int year, int month);

  /**
   * @brief 将日期转换为自1970-01-01以来的天数
   * @param year 年份
   * @param month 月份 (1-12)
   * @param day 天数
   * @return 天数，错误时返回 INT32_MIN
   */
  static int32_t date_to_days(int year, int month, int day);

  /**
   * @brief 将自1970-01-01以来的天数转换为日期
   * @param days 天数
   * @param year 输出年份
   * @param month 输出月份
   * @param day 输出天数
   */
  static void days_to_date(int32_t days, int &year, int &month, int &day);

  /**
   * @brief 解析日期字符串 (YYYY-MM-DD 格式)
   * @param date_str 日期字符串
   * @param year 输出年份
   * @param month 输出月份
   * @param day 输出天数
   * @return RC::SUCCESS 如果解析成功，RC::FAILURE 否则
   */
  static RC parse_date_string(const string &date_str, int &year, int &month, int &day);
};