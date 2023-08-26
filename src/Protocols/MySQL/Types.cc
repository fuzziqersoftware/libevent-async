#include "Types.hh"

#include <stdio.h>

#include <phosg/Hash.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "ProtocolBuffer.hh"

using namespace std;

namespace EventAsync::MySQL {

const char* name_for_column_type(uint8_t type) {
  static const vector<const char*> low_types({
      "T_DECIMAL",
      "T_TINYINT",
      "T_SMALLINT",
      "T_INT",
      "T_FLOAT",
      "T_DOUBLE",
      "T_NULL",
      "T_TIMESTAMP",
      "T_BIGINT",
      "T_MEDIUMINT",
      "T_DATE",
      "T_TIME",
      "T_DATETIME",
      "T_YEAR",
      "T_NEWDATE",
      "T_VARCHAR",
      "T_BIT",
      "T_TIMESTAMP2",
      "T_DATETIME2",
      "T_TIME2",
  });
  static const vector<const char*> high_types({
      "T_BOOL",
      "T_JSON",
      "T_NEWDECIMAL",
      "T_ENUM",
      "T_SET",
      "T_TINYBLOB",
      "T_MEDIUMBLOB",
      "T_LONGBLOB",
      "T_BLOB",
      "T_VAR_STRING",
      "T_STRING",
      "T_GEOMETRY",
  });
  try {
    return (type >= 0xF4) ? high_types.at(type - 0xF4) : low_types.at(type);
  } catch (const out_of_range&) {
    throw runtime_error(string_printf("%02hhX", type));
    return "<INVALID_COLUMN_TYPE>";
  }
}

DateTimeValue::DateTimeValue(const string& str)
    : years(0),
      months(0),
      days(0),
      hours(0),
      minutes(0),
      seconds(0),
      usecs(0) {
  sscanf(str.c_str(), "%hu-%hhu-%hhu %hhu:%hhu:%hhu.%u",
      &this->years, &this->months, &this->days, &this->hours, &this->minutes,
      &this->seconds, &this->usecs);
  // Hack: there might be fewer than six digits of precision on the usecs field,
  // so we have to account for this manually here.
  for (size_t x = str.size(); x < 26; x++) {
    this->usecs *= 10;
  }
}

string DateTimeValue::str() const {
  return string_printf("%04hu-%02hhu-%02hhu %02hhu:%02hhu:%02hhu.%06u",
      this->years, this->months, this->days, this->hours, this->minutes,
      this->seconds, this->usecs);
}

TimeValue::TimeValue(const string& str)
    : is_negative(false),
      hours(0),
      minutes(0),
      seconds(0),
      usecs(0) {

  this->is_negative = str[0] == '-';
  const char* s = is_negative ? &str[1] : &str[0];
  sscanf(s, "%u:%hhu:%hhu.%u",
      &this->hours, &this->minutes, &this->seconds, &this->usecs);

  size_t dot_pos = str.find('.');
  if (dot_pos != string::npos) {
    size_t precision = str.size() - dot_pos - 1;
    sscanf(&str[dot_pos + 1], "%u", &this->usecs);
    for (; precision < 6; precision++) {
      this->usecs *= 10;
    }
  }
}

string TimeValue::str() const {
  return string_printf("%s%02u:%02hhu:%02hhu.%06u",
      this->is_negative ? "-" : "", this->hours, this->minutes, this->seconds,
      this->usecs);
}

vector<vector<Value>>& ResultSet::rows_vecs() {
  return get<vector<vector<Value>>>(this->rows);
}

const vector<vector<Value>>& ResultSet::rows_vecs() const {
  return get<vector<vector<Value>>>(this->rows);
}

vector<unordered_map<string, Value>>& ResultSet::rows_dicts() {
  return get<vector<unordered_map<string, Value>>>(this->rows);
}

const vector<unordered_map<string, Value>>& ResultSet::rows_dicts() const {
  return get<vector<unordered_map<string, Value>>>(this->rows);
}

string format_cell_value(const Value& cell) {
  if (holds_alternative<uint64_t>(cell)) {
    return string_printf("(uint64) %" PRIu64, get<uint64_t>(cell));
  } else if (holds_alternative<int64_t>(cell)) {
    return string_printf("(int64) %" PRId64, get<int64_t>(cell));
  } else if (holds_alternative<float>(cell)) {
    return string_printf("(float) %g", get<float>(cell));
  } else if (holds_alternative<double>(cell)) {
    return string_printf("(double) %lg", get<double>(cell));
  } else if (holds_alternative<const void*>(cell)) {
    return string_printf("(null)");
  } else if (holds_alternative<DateTimeValue>(cell)) {
    string s = get<DateTimeValue>(cell).str();
    return string_printf("(datetime) %s", s.c_str());
  } else if (holds_alternative<TimeValue>(cell)) {
    string s = get<TimeValue>(cell).str();
    return string_printf("(time) %s", s.c_str());
  } else if (holds_alternative<string>(cell)) {
    string s = escape_quotes(get<string>(cell));
    return string_printf("(string) \"%s\"", s.c_str());
  } else {
    return string_printf("(invalid)");
  }
};

void ResultSet::print(FILE* stream) const {
  fprintf(stream, "ResultSet:\n");
  fprintf(stream, "  Metadata:\n");
  fprintf(stream, "    affected_rows: %" PRIu64 "\n", this->affected_rows);
  fprintf(stream, "    insert_id: %" PRIu64 "\n", this->insert_id);
  fprintf(stream, "    status_flags: 0x%04hX\n", this->status_flags);
  fprintf(stream, "    warning_count: %hu\n", this->warning_count);
  fprintf(stream, "  Columns:\n");
  for (const auto& column_def : this->columns) {
    string column_name = column_def.column_name;
    if (!column_def.table_name.empty()) {
      column_name = column_def.table_name + "." + column_name;
      if (!column_def.database_name.empty()) {
        column_name = column_def.database_name + "." + column_name;
      }
    }

    string from_str;
    if (!column_def.original_column_name.empty() &&
        !column_def.original_table_name.empty()) {
      from_str = "from " + column_def.original_table_name + "." + column_def.original_column_name + "; ";
    } else if (!column_def.original_table_name.empty()) {
      from_str = "from " + column_def.original_table_name + "." + column_def.column_name + "; ";
    } else if (!column_def.original_column_name.empty()) {
      from_str = "from " + column_def.original_column_name + "; ";
    }

    fprintf(stream, "    %s (%scharset 0x%04hX; type %02hhX; flags %04hX)\n",
        column_name.c_str(),
        from_str.c_str(),
        column_def.charset,
        static_cast<uint8_t>(column_def.type),
        column_def.flags);
  }

  if (holds_alternative<vector<unordered_map<string, Value>>>(this->rows)) {
    const auto& row_dicts = get<vector<unordered_map<string, Value>>>(this->rows);
    for (const auto& row : row_dicts) {
      fprintf(stream, "  Row (dict):\n");
      for (const auto& cell_it : row) {
        string s = format_cell_value(cell_it.second);
        fprintf(stream, "    %s = %s\n", cell_it.first.c_str(), s.c_str());
      }
    }

  } else {
    const auto& row_vecs = get<vector<vector<Value>>>(this->rows);
    for (const auto& row : row_vecs) {
      fprintf(stream, "  Row (vector):\n");
      for (size_t column_index = 0; column_index < this->columns.size(); column_index++) {
        const auto& column = this->columns[column_index];
        const auto& cell = row.at(column_index);
        string s = format_cell_value(cell);
        fprintf(stream, "    %s = %s\n", column.column_name.c_str(), s.c_str());
      }
    }
  }
}

} // namespace EventAsync::MySQL
