/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <limits>

#include "range/v3/view/transform.hpp"
#include "range/v3/range/conversion.hpp"

#include "utils/Environment.h"
#include "utils/GeneralUtils.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

namespace views = ranges::views;

std::optional<bool> StringUtils::toBool(const std::string& str) {
  std::string trimmed = trim(str);
  if (equalsIgnoreCase(trimmed, "true")) {
    return true;
  }
  if (equalsIgnoreCase(trimmed, "false")) {
    return false;
  }
  return std::nullopt;
}

std::string StringUtils::toLower(std::string_view str) {
  const auto tolower = [](auto c) { return std::tolower(static_cast<unsigned char>(c)); };
  return str | views::transform(tolower) | ranges::to<std::string>();
}

std::pair<std::string, std::string> StringUtils::chomp(const std::string& input_line) {
  if (endsWith(input_line, "\r\n")) {
    return std::make_pair(input_line.substr(0, input_line.size() - 2), "\r\n");
  } else if (endsWith(input_line, "\n")) {
    return std::make_pair(input_line.substr(0, input_line.size() - 1), "\n");
  } else {
    return std::make_pair(input_line, "");
  }
}

std::string StringUtils::trim(const std::string& s) {
  return trimRight(trimLeft(s));
}

std::string_view StringUtils::trim(std::string_view sv) {
  auto begin = std::find_if(sv.begin(), sv.end(), [](unsigned char c) -> bool { return !isspace(c); });
  auto end = std::find_if(sv.rbegin(), std::reverse_iterator(begin), [](unsigned char c) -> bool { return !isspace(c); }).base();
  // c++20 iterator constructor
  // return std::string_view(begin, end);
  // but for now
  // on windows std::string_view::const_iterator is not a const char*
  return sv.substr(std::distance(sv.begin(), begin), std::distance(begin, end));
}

std::string_view StringUtils::trim(const char* str) {
  return trim(std::string_view(str));
}

template<typename Fun>
std::vector<std::string> split_transformed(std::string str, const std::string& delimiter, Fun transformation) {
  std::vector<std::string> result;
  if (delimiter.empty()) {
    for (auto c : str) {
      result.push_back(transformation(std::string(1, c)));
    }
    return result;
  }

  size_t pos = str.find(delimiter);
  if (pos == std::string::npos) {
    result.push_back(transformation(str));
    return result;
  }
  while (pos != std::string::npos) {
    result.push_back(transformation(str.substr(0, pos)));
    str = str.substr(pos + delimiter.size());
    pos = str.find(delimiter);
  }
  result.push_back(transformation(str));
  return result;
}

std::vector<std::string> StringUtils::split(const std::string& str, const std::string& delimiter) {
  return split_transformed(str, delimiter, identity{});
}

std::vector<std::string> StringUtils::splitRemovingEmpty(const std::string& str, const std::string& delimiter) {
  auto result = split(str, delimiter);
  result.erase(std::remove_if(result.begin(), result.end(), [](const std::string& str) { return str.empty(); }), result.end());
  return result;
}

std::vector<std::string> StringUtils::splitAndTrim(const std::string& str, const std::string& delimiter) {
  return split_transformed(str, delimiter, static_cast<std::string(*)(const std::string&)>(trim));
}

std::vector<std::string> StringUtils::splitAndTrimRemovingEmpty(const std::string& str, const std::string& delimiter) {
  auto result = splitAndTrim(str, delimiter);
  result.erase(std::remove_if(result.begin(), result.end(), [](const std::string& str) { return str.empty(); }), result.end());
  return result;
}

bool StringUtils::StringToFloat(std::string input, float &output, FailurePolicy cp /*= RETURN*/) {
  try {
    output = std::stof(input);
  } catch (const std::invalid_argument &ie) {
    switch (cp) {
      case RETURN:
      case NOTHING:
        return false;
      case EXIT:
        exit(1);
      case EXCEPT:
        throw ie;
    }
  } catch (const std::out_of_range &ofr) {
    switch (cp) {
      case RETURN:
      case NOTHING:
        return false;
      case EXIT:
        exit(1);
      case EXCEPT:
        throw ofr;
    }
  }

  return true;
}

std::string StringUtils::replaceEnvironmentVariables(std::string source_string) {
  std::string::size_type beg_seq = 0;
  std::string::size_type end_seq = 0;
  do {
    beg_seq = source_string.find("${", beg_seq);
    if (beg_seq == std::string::npos) {
      break;
    }
    if (beg_seq > 0 && source_string.at(beg_seq - 1) == '\\') {
      beg_seq += 2;
      continue;
    }
    end_seq = source_string.find("}", beg_seq + 2);
    if (end_seq == std::string::npos) {
      break;
    }
    if (end_seq <= beg_seq + 2) {
      beg_seq += 2;
      continue;
    }
    auto env_var_length = end_seq - (beg_seq + 2);
    const std::string env_var = source_string.substr(beg_seq + 2, env_var_length);
    const std::string env_var_wrapped = source_string.substr(beg_seq, env_var_length + 3);

    std::string env_value;
    std::tie(std::ignore, env_value) = utils::Environment::getEnvironmentVariable(env_var.c_str());

    source_string = replaceAll(source_string, env_var_wrapped, env_value);
    beg_seq = 0;  // restart
  } while (beg_seq < source_string.size());

  source_string = replaceAll(source_string, "\\$", "$");

  return source_string;
}

std::string StringUtils::replaceOne(const std::string &input, const std::string &from, const std::string &to) {
  std::size_t found_at_position = input.find(from);
  if (found_at_position != std::string::npos) {
    std::string input_copy = input;
    return input_copy.replace(found_at_position, from.size(), to);
  } else {
    return input;
  }
}

std::string& StringUtils::replaceAll(std::string& source_string, const std::string &from_string, const std::string &to_string) {
  std::size_t loc = 0;
  std::size_t lastFound;
  while ((lastFound = source_string.find(from_string, loc)) != std::string::npos) {
    source_string.replace(lastFound, from_string.size(), to_string);
    loc = lastFound + to_string.size();
    if (from_string.empty()) {
      loc++;
    }
  }
  return source_string;
}

std::string StringUtils::replaceMap(std::string source_string, const std::map<std::string, std::string> &replace_map) {
  auto result_string = source_string;

  std::vector<std::pair<size_t, std::pair<size_t, std::string>>> replacements;
  for (const auto &replace_pair : replace_map) {
    size_t replace_pos = 0;
    while ((replace_pos = source_string.find(replace_pair.first, replace_pos)) != std::string::npos) {
      replacements.emplace_back(std::make_pair(replace_pos, std::make_pair(replace_pair.first.length(), replace_pair.second)));
      replace_pos += replace_pair.first.length();
    }
  }

  std::sort(replacements.begin(), replacements.end(), [](const std::pair<size_t, std::pair<size_t, std::string>> a,
                                                         const std::pair<size_t, std::pair<size_t, std::string>> &b) {
    return a.first > b.first;
  });

  for (const auto &replacement : replacements) {
    result_string = source_string.replace(replacement.first, replacement.second.first, replacement.second.second);
  }

  return result_string;
}

bool StringUtils::from_hex(uint8_t ch, uint8_t& output) {
  if (ch > 127) {
    return false;
  }
  output = hex_lut[ch];
  return output != SKIP;
}

bool StringUtils::from_hex(uint8_t* data, size_t* data_length, const char* hex, size_t hex_length) {
  if (*data_length < hex_length / 2) {
    return false;
  }
  uint8_t n1;
  bool found_first_nibble = false;
  *data_length = 0;
  for (size_t i = 0; i < hex_length; i++) {
    const uint8_t byte = static_cast<uint8_t>(hex[i]);
    if (byte > 127) {
      continue;
    }
    uint8_t n = hex_lut[byte];
    if (n != SKIP) {
      if (found_first_nibble) {
        data[(*data_length)++] = n1 << 4 | n;
        found_first_nibble = false;
      } else {
        n1 = n;
        found_first_nibble = true;
      }
    }
  }
  if (found_first_nibble) {
    return false;
  }
  return true;
}

std::vector<uint8_t> StringUtils::from_hex(const char* hex, size_t hex_length) {
  std::vector<uint8_t> decoded(hex_length / 2);
  size_t data_length = decoded.size();
  if (!from_hex(decoded.data(), &data_length, hex, hex_length)) {
    throw std::invalid_argument("Hexencoded string is malformatted");
  }
  decoded.resize(data_length);
  return decoded;
}

size_t StringUtils::to_hex(char* hex, const uint8_t* data, size_t length, bool uppercase) {
  if (length > std::numeric_limits<size_t>::max() / 2) {
    throw std::length_error("Data is too large to be hexencoded");
  }
  for (size_t i = 0; i < length; i++) {
    hex[i * 2] = nibble_to_hex(data[i] >> 4, uppercase);
    hex[i * 2 + 1] = nibble_to_hex(data[i] & 0xf, uppercase);
  }
  return length * 2;
}

std::string StringUtils::to_hex(const uint8_t* data, size_t length, bool uppercase /*= false*/) {
  if (length > (std::numeric_limits<size_t>::max() / 2 - 1)) {
    throw std::length_error("Data is too large to be hexencoded");
  }
  std::vector<char> buf(length * 2);
  const size_t hex_length = to_hex(buf.data(), data, length, uppercase);
  return std::string(buf.data(), hex_length);
}

bool StringUtils::from_base64(uint8_t* data, size_t* data_length, const char* base64, size_t base64_length) {
  if (*data_length < (base64_length / 4 + 1) * 3) {
    return false;
  }

  uint8_t digits[4];
  size_t digit_counter = 0U;
  size_t decoded_size = 0U;
  size_t padding_counter = 0U;
  size_t i;
  for (i = 0U; i < base64_length; i++) {
    const uint8_t byte = static_cast<uint8_t>(base64[i]);
    if (byte > 127) {
      return false;
    }

    const uint8_t decoded = base64_dec_lut[byte];
    switch (decoded) {
      case SKIP:
        continue;
      case ILGL:
        return false;
      case PDNG:
        padding_counter++;
        continue;
      default:
        if (padding_counter > 0U) {
          return false;
        }
        digits[digit_counter++] = decoded;
        if (digit_counter == 4U) {
          base64_digits_to_bytes(digits, data + decoded_size);
          decoded_size += 3U;
          digit_counter = 0U;
        }
    }
  }

  if (padding_counter > 0U && padding_counter != 4U - digit_counter) {
    return false;
  }

  switch (digit_counter) {
    case 0:
      break;
    case 1:
      return false;
    case 2:
      digits[2] = 0x00;
      [[fallthrough]];
    case 3: {
      digits[3] = 0x00;

      uint8_t bytes_temp[3];
      base64_digits_to_bytes(digits, bytes_temp);
      const size_t num_bytes = digit_counter - 1;
      memcpy(data + decoded_size, bytes_temp, num_bytes);
      decoded_size += num_bytes;
      break;
    }
    default:
      return false;
  }

  *data_length = decoded_size;
  return true;
}

std::vector<uint8_t> StringUtils::from_base64(const char* base64, size_t base64_length) {
  std::vector<uint8_t> decoded((base64_length / 4 + 1) * 3);
  size_t data_length = decoded.size();
  if (!from_base64(decoded.data(), &data_length, base64, base64_length)) {
    throw std::invalid_argument("Base64 encoded string is malformatted");
  }
  decoded.resize(data_length);
  return decoded;
}

size_t StringUtils::to_base64(char* base64, const uint8_t* data, size_t length, bool url, bool padded) {
  if (length > std::numeric_limits<size_t>::max() * 3 / 4 - 3) {
    throw std::length_error("Data is too large to be base64 encoded");
  }

  const char* enc_lut = url ? base64_url_enc_lut : base64_enc_lut;
  size_t base64_length = 0U;
  uint8_t bytes[3];
  for (size_t i = 0U; i < length; i += 3U) {
    const bool b1_present = i + 1 < length;
    const bool b2_present = i + 2 < length;
    bytes[0] = data[i];
    bytes[1] = b1_present ? data[i + 1] : 0x00;
    bytes[2] = b2_present ? data[i + 2] : 0x00;

    base64[base64_length++] = enc_lut[(bytes[0] & 0xfc) >> 2];
    base64[base64_length++] = enc_lut[(bytes[0] & 0x03) << 4 | (bytes[1] & 0xf0) >> 4];
    if (b1_present) {
      base64[base64_length++] = enc_lut[(bytes[1] & 0x0f) << 2 | (bytes[2] & 0xc0) >> 6];
    } else if (padded) {
      base64[base64_length++] = '=';
    }
    if (b2_present) {
      base64[base64_length++] = enc_lut[bytes[2] & 0x3f];
    } else if (padded) {
      base64[base64_length++] = '=';
    }
  }

  return base64_length;
}

std::string StringUtils::to_base64(const uint8_t* data, size_t length, bool url /*= false*/, bool padded /*= true*/) {
  std::vector<char> buf((length / 3 + 1) * 4);
  size_t base64_length = to_base64(buf.data(), data, length, url, padded);
  return std::string(buf.data(), base64_length);
}

std::smatch StringUtils::getLastRegexMatch(const std::string& str, const std::regex& pattern) {
  auto matches = std::sregex_iterator(str.begin(), str.end(), pattern);
  std::smatch last_match;
  while (matches != std::sregex_iterator()) {
    last_match = *matches;
    matches = std::next(matches);
  }
  return last_match;
}

constexpr uint8_t StringUtils::SKIP;
constexpr uint8_t StringUtils::hex_lut[128];
constexpr const char StringUtils::base64_enc_lut[];
constexpr const char StringUtils::base64_url_enc_lut[];
constexpr uint8_t StringUtils::base64_dec_lut[128];

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
