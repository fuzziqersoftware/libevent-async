#include "Client.hh"

#include <stdio.h>
#include <event2/buffer.h>

#include <phosg/Hash.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "ProtocolBuffer.hh"

using namespace std;



namespace EventAsync::MySQL {

DateTimeValue::DateTimeValue(const string& str)
  : years(0), months(0), days(0), hours(0), minutes(0), seconds(0), usecs(0) {
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
  : is_negative(false), days(0), hours(0), minutes(0), seconds(0), usecs(0) {
  // TODO
  throw logic_error("MySQL::TimeValue is unimplemented");
}

string TimeValue::str() const {
  return string_printf("%s%02hhu:%02hhu:%02hhu.%06u",
      this->is_negative ? "-" : "", this->hours + this->days * 24,
      this->minutes, this->seconds, this->usecs);
}



void ResultSet::print(FILE* stream) const {
  fprintf(stream, "ResultSet:\n");
  fprintf(stream, "  Metadata:\n");
  fprintf(stream, "    affected_rows: %llu\n", this->affected_rows);
  fprintf(stream, "    insert_id: %llu\n", this->insert_id);
  fprintf(stream, "    status_flags: 0x%04hX\n", this->status_flags);
  fprintf(stream, "    warning_count: %hu\n", this->warning_count);
  fprintf(stream, "  Columns:\n");
  for (const auto& column_def : this->columns) {
    fprintf(stream, "    %s.%s.%s (from %s.%s; charset 0x%04hX; type %02hhX; flags %04hX)\n",
        column_def.database_name.c_str(),
        column_def.table_name.c_str(),
        column_def.column_name.c_str(),
        column_def.original_table_name.c_str(),
        column_def.original_column_name.c_str(),
        column_def.charset,
        static_cast<uint8_t>(column_def.type),
        column_def.flags);
  }
  for (const auto& row : this->rows) {
    fprintf(stream, "  Row:\n");
    for (size_t column_index = 0; column_index < this->columns.size(); column_index++) {
      const auto& column = this->columns[column_index];
      const auto& cell = row.at(column_index);
      if (holds_alternative<uint64_t>(cell)) {
        fprintf(stream, "    %s = (uint64) %llu\n", column.column_name.c_str(), get<uint64_t>(cell));
      } else if (holds_alternative<int64_t>(cell)) {
        fprintf(stream, "    %s = (int64) %lld\n", column.column_name.c_str(), get<int64_t>(cell));
      } else if (holds_alternative<float>(cell)) {
        fprintf(stream, "    %s = (float) %g\n", column.column_name.c_str(), get<float>(cell));
      } else if (holds_alternative<double>(cell)) {
        fprintf(stream, "    %s = (double) %lg\n", column.column_name.c_str(), get<double>(cell));
      } else if (holds_alternative<const void*>(cell)) {
        fprintf(stream, "    %s = (null)\n", column.column_name.c_str());
      } else if (holds_alternative<DateTimeValue>(cell)) {
        string s = get<DateTimeValue>(cell).str();
        fprintf(stream, "    %s = (datetime) %s\n", column.column_name.c_str(), s.c_str());
      } else if (holds_alternative<TimeValue>(cell)) {
        string s = get<TimeValue>(cell).str();
        fprintf(stream, "    %s = (time) %s\n", column.column_name.c_str(), s.c_str());
      } else if (holds_alternative<string>(cell)) {
        fprintf(stream, "    %s = (string) %s\n", column.column_name.c_str(), get<string>(cell).c_str());
      } else {
        fprintf(stream, "    %s = (invalid)\n", column.column_name.c_str());
      }
    }
  }
}

} // namespace EventAsync::MySQL
