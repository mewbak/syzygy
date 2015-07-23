// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Much of this file has been adapted from Chromium (net/http/http_util.cc) and
// Breakpad (common/linux/http_upload.cc).
// See http://www.ietf.org/rfc/rfc2388.txt for a description of the
// multipart/form-data HTTP message type implemented in this file.
#include "syzygy/kasko/internet_helpers.h"

#include <winhttp.h>  // NOLINT

#include <wchar.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace kasko {

namespace {

// Returns the index of the closing quote of the string, if any. |start| points
// at the opening quote.
size_t FindStringEnd(const base::string16& line,
                     size_t start,
                     base::char16 delim) {
  DCHECK_LT(start, line.length());
  DCHECK_EQ(line[start], delim);
  DCHECK((delim == L'"') || (delim == L'\''));

  const base::char16 set[] = { delim, L'\\', L'\0' };
  for (size_t end = line.find_first_of(set, start + 1);
       end != base::string16::npos; end = line.find_first_of(set, end + 2)) {
    if (line[end] != L'\\')
      return end;
  }
  return line.length();
}

const base::char16 kHttpLws[] = L" \t";

bool IsLWS(base::char16 c) {
 return ::wcschr(kHttpLws, c) != NULL;
}

void TrimLWS(base::string16::const_iterator* begin,
             base::string16::const_iterator* end) {
  // Skip leading whitespace.
  while (*begin < *end && IsLWS((*begin)[0]))
    ++(*begin);

  // Skip trailing whitespace.
  while (*begin < *end && IsLWS((*end)[-1]))
    --(*end);
}

}  // namespace

void ParseContentType(const base::string16& content_type_str,
                      base::string16* mime_type,
                      base::string16* charset,
                      bool* had_charset,
                      base::string16* boundary) {
  const base::string16::const_iterator begin = content_type_str.begin();

  // Trim leading and trailing whitespace from type. We include '(' in the
  // trailing trim set to catch media-type comments, which are not at all
  // standard, but may occur in rare cases.
  size_t type_val = content_type_str.find_first_not_of(kHttpLws);
  type_val = std::min(type_val, content_type_str.length());
  size_t type_end = content_type_str.find_first_of(
      base::string16(kHttpLws) + L";(", type_val);
  if (type_end == base::string16::npos)
    type_end = content_type_str.length();

  size_t charset_val = 0;
  size_t charset_end = 0;
  bool type_has_charset = false;

  // Iterate over parameters.
  size_t param_start = content_type_str.find_first_of(';', type_end);
  if (param_start != std::string::npos) {
    base::StringTokenizerT<base::string16, base::string16::const_iterator>
        tokenizer(begin + param_start, content_type_str.end(), L";");
    tokenizer.set_quote_chars(L"\"");
    while (tokenizer.GetNext()) {
      base::string16::const_iterator equals_sign =
          std::find(tokenizer.token_begin(), tokenizer.token_end(), L'=');
      if (equals_sign == tokenizer.token_end())
        continue;

      base::string16::const_iterator param_name_begin = tokenizer.token_begin();
      base::string16::const_iterator param_name_end = equals_sign;
      TrimLWS(&param_name_begin, &param_name_end);

      base::string16::const_iterator param_value_begin = equals_sign + 1;
      base::string16::const_iterator param_value_end = tokenizer.token_end();
      DCHECK(param_value_begin <= tokenizer.token_end());
      TrimLWS(&param_value_begin, &param_value_end);

      if (base::LowerCaseEqualsASCII(param_name_begin, param_name_end,
                                     "charset")) {
        charset_val = param_value_begin - begin;
        charset_end = param_value_end - begin;
        type_has_charset = true;
      } else if (base::LowerCaseEqualsASCII(param_name_begin, param_name_end,
                                            "boundary")) {
        if (boundary)
          boundary->assign(param_value_begin, param_value_end);
      }
    }
  }

  if (type_has_charset) {
    // Trim leading and trailing whitespace from charset_val. We include '(' in
    // the trailing trim set to catch media-type comments, which are not at all
    // standard, but may occur in rare cases.
    charset_val = content_type_str.find_first_not_of(kHttpLws, charset_val);
    charset_val = std::min(charset_val, charset_end);
    base::char16 first_char = content_type_str[charset_val];
    if (first_char == L'"' || first_char == L'\'') {
      charset_end = FindStringEnd(content_type_str, charset_val, first_char);
      ++charset_val;
      DCHECK(charset_end >= charset_val);
    } else {
      charset_end = std::min(content_type_str.find_first_of(
                                 base::string16(kHttpLws) + L";(", charset_val),
                             charset_end);
    }
  }

  // If the server sent "*/*", it is meaningless, so do not store it.
  // Also, if type_val is the same as mime_type, then just update the charset
  // However, if charset is empty and mime_type hasn't changed, then don't
  // wipe-out an existing charset. We also want to reject a mime-type if it does
  // not include a slash. Some servers give junk after the charset parameter,
  // which may include a comma, so this check makes us a bit more tolerant.
  if (content_type_str.length() != 0 &&
      content_type_str != L"*/*" &&
      content_type_str.find_first_of(L'/') != base::string16::npos) {
    // The common case here is that mime_type is empty.
    bool eq = !mime_type->empty() &&
              base::LowerCaseEqualsASCII(begin + type_val, begin + type_end,
                                         base::WideToUTF8(*mime_type).data());
    if (!eq) {
      mime_type->assign(begin + type_val, begin + type_end);
      base::StringToLowerASCII(mime_type);
    }
    if ((!eq && *had_charset) || type_has_charset) {
      *had_charset = true;
      charset->assign(begin + charset_val, begin + charset_end);
      base::StringToLowerASCII(charset);
    }
  }
}

bool DecomposeUrl(const base::string16& url,
                  base::string16* scheme,
                  base::string16* host,
                  uint16_t* port,
                  base::string16* path) {
  DCHECK(scheme);
  DCHECK(host);
  DCHECK(path);

  wchar_t scheme_buffer[16], host_buffer[256], path_buffer[256];
  URL_COMPONENTS components;
  memset(&components, 0, sizeof(components));
  components.dwStructSize = sizeof(components);
  components.lpszScheme = scheme_buffer;
  components.dwSchemeLength = sizeof(scheme_buffer) / sizeof(scheme_buffer[0]);
  components.lpszHostName = host_buffer;
  components.dwHostNameLength = sizeof(host_buffer) / sizeof(host_buffer[0]);
  components.lpszUrlPath = path_buffer;
  components.dwUrlPathLength = sizeof(path_buffer) / sizeof(path_buffer[0]);
  if (!::WinHttpCrackUrl(url.c_str(), 0, 0, &components))
    return false;
  *scheme = scheme_buffer;
  *host = host_buffer;
  *path = path_buffer;
  *port = components.nPort;
  return true;
}

base::string16 ComposeUrl(const base::string16& host,
                          uint16_t port,
                          const base::string16& path,
                          bool secure) {
  if (secure) {
    if (port == 443)
      return L"https://" + host + path;
    return L"https://" + host + L':' + base::UintToString16(port) + path;
  }
  if (port == 80)
      return L"http://" + host + path;
  return L"http://" + host + L':' + base::UintToString16(port) + path;
}

base::string16 GenerateMultipartHttpRequestBoundary() {
  // The boundary has 27 '-' characters followed by 16 hex digits.
  static const base::char16 kBoundaryPrefix[] = L"---------------------------";
  static const size_t kBoundaryLength = 27 + 16;

  // Generate some random numbers to fill out the boundary.
  int r0 = rand();
  int r1 = rand();

  // Add one character for the NULL termination.
  base::char16 temp[kBoundaryLength + 1];
  ::swprintf(temp, sizeof(temp) / sizeof(*temp), L"%s%08X%08X", kBoundaryPrefix,
             r0, r1);

  return base::string16(temp, kBoundaryLength);
}

base::string16 GenerateMultipartHttpRequestContentTypeHeader(
    const base::string16 boundary) {
  return L"Content-Type: multipart/form-data; boundary=" + boundary;
}

std::string GenerateMultipartHttpRequestBody(
    const std::map<base::string16, base::string16>& parameters,
    const std::string& upload_file,
    const base::string16& file_part_name,
    const base::string16& boundary) {
  DCHECK(!boundary.empty());
  DCHECK(!file_part_name.empty());
  std::string boundary_utf8 = base::WideToUTF8(boundary);

  std::string request_body;

  // Append each of the parameter pairs as a form-data part.
  for (const auto& entry : parameters) {
    request_body.append("--" + boundary_utf8 + "\r\n");
    request_body.append("Content-Disposition: form-data; name=\"" +
                        base::WideToUTF8(entry.first) + "\"\r\n\r\n" +
                        base::WideToUTF8(entry.second) + "\r\n");
  }

  std::string file_part_name_utf8 = base::WideToUTF8(file_part_name);

  request_body.append("--" + boundary_utf8 + "\r\n");
  request_body.append("Content-Disposition: form-data; "
                      "name=\"" + file_part_name_utf8 + "\"; "
                      "filename=\"" + file_part_name_utf8 + "\"\r\n");
  request_body.append("Content-Type: application/octet-stream\r\n");
  request_body.append("\r\n");

  request_body.append(upload_file);
  request_body.append("\r\n");
  request_body.append("--" + boundary_utf8 + "--\r\n");

  return request_body;
}

}  // namespace kasko
