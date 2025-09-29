/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by WangYunlai on 2023/06/28.
//

#include "common/value.h"

#include "common/lang/comparator.h"
#include "common/lang/exception.h"
#include "common/lang/sstream.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "common/time/datetime.h"
#include "value.h"

Value::Value(int val) { set_int(val); }

Value::Value(float val) { set_float(val); }

Value::Value(bool val) { set_boolean(val); }

Value::Value(const char *s, int len /*= 0*/) { set_string(s, len); }

Value::Value(const string_t& s) { set_string(s.data(), s.size()); }


Value::Value(const Value &other)
{
  this->attr_type_ = other.attr_type_;
  this->length_    = other.length_;
  this->own_data_  = other.own_data_;
  switch (this->attr_type_) {
    case AttrType::CHARS: {
      set_string_from_other(other);
    } break;
    case AttrType::DATES: {
      set_date_from_other(other);
    } break;

    default: {
      this->value_ = other.value_;
    } break;
  }
}

Value::Value(Value &&other)
{
  this->attr_type_ = other.attr_type_;
  this->length_    = other.length_;
  this->own_data_  = other.own_data_;
  this->value_     = other.value_;
  other.own_data_  = false;
  other.length_    = 0;
}

Value &Value::operator=(const Value &other)
{
  if (this == &other) {
    return *this;
  }
  reset();
  this->attr_type_ = other.attr_type_;
  this->length_    = other.length_;
  this->own_data_  = other.own_data_;
  switch (this->attr_type_) {
    case AttrType::CHARS: {
      set_string_from_other(other);
    } break;

    default: {
      this->value_ = other.value_;
    } break;
  }
  return *this;
}

Value &Value::operator=(Value &&other)
{
  if (this == &other) {
    return *this;
  }
  reset();
  this->attr_type_ = other.attr_type_;
  this->length_    = other.length_;
  this->own_data_  = other.own_data_;
  this->value_     = other.value_;
  other.own_data_  = false;
  other.length_    = 0;
  return *this;
}

void Value::reset()
{
  switch (attr_type_) {
    case AttrType::CHARS:
      if (own_data_ && value_.pointer_value_ != nullptr) {
        delete[] value_.pointer_value_;
        value_.pointer_value_ = nullptr;
      }
      break;
    default: break;
  }

  attr_type_ = AttrType::UNDEFINED;
  length_    = 0;
  own_data_  = false;
}

void Value::set_data(char *data, int length)
{
  switch (attr_type_) {
    case AttrType::CHARS: {
      set_string(data, length);
    } break;
    case AttrType::INTS: {
      value_.int_value_ = *(int *)data;
      length_           = length;
    } break;
    case AttrType::FLOATS: {
      value_.float_value_ = *(float *)data;
      length_             = length;
    } break;
    case AttrType::BOOLEANS: {
      value_.bool_value_ = *(int *)data != 0;
      length_            = length;
    } break;
    case AttrType::DATES: {
      value_.int_value_ = *(int32_t *)data;  // 存储为天数
      length_           = length;
    } break;
    default: {
      LOG_WARN("unknown data type: %d", attr_type_);
    } break;
  }
}

void Value::set_int(int val)
{
  reset();
  attr_type_        = AttrType::INTS;
  value_.int_value_ = val;
  length_           = sizeof(val);
}

void Value::set_float(float val)
{
  reset();
  attr_type_          = AttrType::FLOATS;
  value_.float_value_ = val;
  length_             = sizeof(val);
}
void Value::set_boolean(bool val)
{
  reset();
  attr_type_         = AttrType::BOOLEANS;
  value_.bool_value_ = val;
  length_            = sizeof(val);
}

void Value::set_string(const char *s, int len /*= 0*/)
{
  reset();
  attr_type_ = AttrType::CHARS;
  if (s == nullptr) {
    value_.pointer_value_ = nullptr;
    length_               = 0;
  } else {
    own_data_ = true;
    if (len > 0) {
      len = strnlen(s, len);
    } else {
      len = strlen(s);
    }
    value_.pointer_value_ = new char[len + 1];
    length_               = len;
    memcpy(value_.pointer_value_, s, len);
    value_.pointer_value_[len] = '\0';
  }
}

void Value::set_empty_string(int len)
{
  reset();
  attr_type_ = AttrType::CHARS;

  own_data_ = true;
  value_.pointer_value_ = new char[len + 1];
  length_               = len;
  memset(value_.pointer_value_, 0, len);
  value_.pointer_value_[len] = '\0';
  
}

void Value::set_value(const Value &value)
{
  switch (value.attr_type_) {
    case AttrType::INTS: {
      set_int(value.get_int());
    } break;
    case AttrType::FLOATS: {
      set_float(value.get_float());
    } break;
    case AttrType::CHARS: {
      set_string(value.get_string().c_str());
    } break;
    case AttrType::BOOLEANS: {
      set_boolean(value.get_boolean());
    } break;
    case AttrType::DATES: {
      set_int(value.get_int());  // 复制天数值
      attr_type_ = AttrType::DATES;  // 确保类型正确
    } break;
    default: {
      ASSERT(false, "got an invalid value type");
    } break;
  }
}

void Value::set_string_from_other(const Value &other)
{
  ASSERT(attr_type_ == AttrType::CHARS, "attr type is not CHARS");
  if (own_data_ && other.value_.pointer_value_ != nullptr && length_ != 0) {
    this->value_.pointer_value_ = new char[this->length_ + 1];
    memcpy(this->value_.pointer_value_, other.value_.pointer_value_, this->length_);
    this->value_.pointer_value_[this->length_] = '\0';
  }
}

char *Value::data() const
{
  switch (attr_type_) {
    case AttrType::CHARS: {
      return value_.pointer_value_;
    } break;
    default: {
      return (char *)&value_;
    } break;
  }
}

string Value::to_string() const
{
  string res;
  RC     rc = DataType::type_instance(this->attr_type_)->to_string(*this, res);
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to convert value to string. type=%s", attr_type_to_string(this->attr_type_));
    return "";
  }
  return res;
}

int Value::compare(const Value &other) const { return DataType::type_instance(this->attr_type_)->compare(*this, other); }

int Value::get_int() const
{
  switch (attr_type_) {
    case AttrType::CHARS: {
      try {
        return (int)(stol(value_.pointer_value_));
      } catch (exception const &ex) {
        LOG_TRACE("failed to convert string to number. s=%s, ex=%s", value_.pointer_value_, ex.what());
        return 0;
      }
    }
    case AttrType::INTS: {
      return value_.int_value_;
    }
    case AttrType::FLOATS: {
      return (int)(value_.float_value_);
    }
    case AttrType::BOOLEANS: {
      return (int)(value_.bool_value_);
    }
    case AttrType::DATES: {
      return value_.int_value_;  // 返回天数
    }
    default: {
      LOG_WARN("unknown data type. type=%d", attr_type_);
      return 0;
    }
  }
  return 0;
}

float Value::get_float() const
{
  switch (attr_type_) {
    case AttrType::CHARS: {
      try {
        return stof(value_.pointer_value_);
      } catch (exception const &ex) {
        LOG_TRACE("failed to convert string to float. s=%s, ex=%s", value_.pointer_value_, ex.what());
        return 0.0;
      }
    } break;
    case AttrType::INTS: {
      return float(value_.int_value_);
    } break;
    case AttrType::FLOATS: {
      return value_.float_value_;
    } break;
    case AttrType::BOOLEANS: {
      return float(value_.bool_value_);
    } break;
    case AttrType::DATES: {
      return float(value_.int_value_);  // 日期作为天数返回
    } break;
    default: {
      LOG_WARN("unknown data type. type=%d", attr_type_);
      return 0;
    }
  }
  return 0;
}

string Value::get_string() const { return this->to_string(); }

string_t Value::get_string_t() const
{
  ASSERT(attr_type_ == AttrType::CHARS, "attr type is not CHARS");
  return string_t(value_.pointer_value_, length_);
}

bool Value::get_boolean() const
{
  switch (attr_type_) {
    case AttrType::CHARS: {
      try {
        float val = stof(value_.pointer_value_);
        if (val >= EPSILON || val <= -EPSILON) {
          return true;
        }

        int int_val = stol(value_.pointer_value_);
        if (int_val != 0) {
          return true;
        }

        return value_.pointer_value_ != nullptr;
      } catch (exception const &ex) {
        LOG_TRACE("failed to convert string to float or integer. s=%s, ex=%s", value_.pointer_value_, ex.what());
        return value_.pointer_value_ != nullptr;
      }
    } break;
    case AttrType::INTS: {
      return value_.int_value_ != 0;
    } break;
    case AttrType::FLOATS: {
      float val = value_.float_value_;
      return val >= EPSILON || val <= -EPSILON;
    } break;
    case AttrType::BOOLEANS: {
      return value_.bool_value_;
    } break;
    case AttrType::DATES: {
      return value_.int_value_ != 0;  // 非零日期为true
    } break;
    default: {
      LOG_WARN("unknown data type. type=%d", attr_type_);
      return false;
    }
  }
  return false;
}

void Value::set_date(int32_t val)
{
  reset();
  attr_type_        = AttrType::DATES;
  value_.int_value_ = val;
  length_           = sizeof(val);
}

void Value::set_date_from_other(const Value &other)
{
  ASSERT(attr_type_ == AttrType::DATES, "attr type is not DATES");
  switch (other.attr_type_) {
    case AttrType::DATES: {
      set_date(other.get_date());
    } break;
    // case AttrType::CHARS: {
    //   // 尝试从字符串解析日期
    //   Value temp_val;
    //   temp_val.set_type(AttrType::DATES);
    //   RC rc = DataType::type_instance(AttrType::DATES)->set_value_from_str(temp_val, other.value_.pointer_value_);
    //   if (rc == RC::SUCCESS) {
    //     set_date(*(int32_t *)temp_val.data());
    //   } else {
    //     LOG_TRACE("failed to convert string to date. s=%s", other.value_.pointer_value_);
    //     set_date(-1);  // 设置为null日期
    //   }
    // } break;
    default: {
      printf("???????");
      LOG_WARN("cannot convert %s to date", attr_type_to_string(other.attr_type_));
      set_date(-1);  // 设置为null日期
    } break;
  }
}

Value*  Value::try_set_date_from_string(const char *s, int len) {
  Value* result = new Value();
  if (len == 0 || s == nullptr) {
    result->set_date(-1);  // 空字符串，设置为null日期
    return result;
  }
  // 检查日期是否合法，输入保证格式合法
  // date测试不会超过2038年2月，不会小于1970年1月1号
  auto check_date = [](int year, int month, int day) {
    if (year < 1970 || (year > 2038 && month >= 2)) {
      return false;
    }
    if (month < 1 || month > 12) {
      return false;
    }
    if (day < 1 || day > 31) {
      return false;
    }
    // 检查闰月
    if (month == 2) {
      bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
      if (is_leap) {
        return day <= 29;
      } else {
        return day <= 28;
      }
    }
    // 检查30天的月份
    if (month == 4 || month == 6 || month == 9 || month == 11) {
      return day <= 30;
    }

    return true;
  };
  int year = 0, month = 0, day = 0;
  sscanf(s, "%4d-%2d-%2d", &year, &month, &day);
  if (check_date(year, month, day)) {
    // 使用 common::DateTime 的方法计算距离1970-1-1的天数
    int julian_day = common::DateTime::julian_date(year, month, day);
    long long days = julian_day - common::DateTime::JULIAN_19700101;
    result->set_date(static_cast<int32_t>(days));
  } else {
    result->set_string(s, len);  // 非法日期，存为字符串
  }
  return result;
}
int32_t Value::get_date() const
{
  switch (attr_type_) {
    case AttrType::DATES: {
      return value_.int_value_;
    }
    case AttrType::CHARS: {
      // 尝试从字符串解析日期
      Value temp_val;
      temp_val.set_type(AttrType::DATES);
      RC rc = DataType::type_instance(AttrType::DATES)->set_value_from_str(temp_val, value_.pointer_value_);
      if (rc == RC::SUCCESS) {
        return *(int32_t *)temp_val.data();
      }
      LOG_TRACE("failed to convert string to date. s=%s", value_.pointer_value_);
      return 0;
    }
    default: {
      LOG_WARN("cannot convert %s to date", attr_type_to_string(attr_type_));
      return 0;
    }
  }
}
