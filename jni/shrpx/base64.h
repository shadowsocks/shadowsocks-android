/*
 * Spdylay - SPDY Library
 *
 * Copyright (c) 2013 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef BASE64_H
#define BASE64_H

#include <string>

namespace spdylay {

namespace base64 {

template<typename InputIterator>
std::string encode(InputIterator first, InputIterator last)
{
  static const char CHAR_TABLE[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/',
  };
  std::string res;
  size_t len = last-first;
  if(len == 0) {
    return res;
  }
  size_t r = len%3;
  InputIterator j = last-r;
  char temp[4];
  while(first != j) {
    int n = static_cast<unsigned char>(*first++) << 16;
    n += static_cast<unsigned char>(*first++) << 8;
    n += static_cast<unsigned char>(*first++);
    temp[0] = CHAR_TABLE[n >> 18];
    temp[1] = CHAR_TABLE[(n >> 12) & 0x3fu];
    temp[2] = CHAR_TABLE[(n >> 6) & 0x3fu];
    temp[3] = CHAR_TABLE[n & 0x3fu];
    res.append(temp, sizeof(temp));
  }
  if(r == 2) {
    int n = static_cast<unsigned char>(*first++) << 16;
    n += static_cast<unsigned char>(*first++) << 8;
    temp[0] = CHAR_TABLE[n >> 18];
    temp[1] = CHAR_TABLE[(n >> 12) & 0x3fu];
    temp[2] = CHAR_TABLE[(n >> 6) & 0x3fu];
    temp[3] = '=';
    res.append(temp, sizeof(temp));
  } else if(r == 1) {
    int n = static_cast<unsigned char>(*first++) << 16;
    temp[0] = CHAR_TABLE[n >> 18];
    temp[1] = CHAR_TABLE[(n >> 12) & 0x3fu];
    temp[2] = '=';
    temp[3] = '=';
    res.append(temp, sizeof(temp));
  }
  return res;
}

template<typename InputIterator>
InputIterator getNext
(InputIterator first,
 InputIterator last,
 const int* tbl)
{
  for(; first != last; ++first) {
    if(tbl[static_cast<size_t>(*first)] != -1 || *first == '=') {
      break;
    }
  }
  return first;
}

template<typename InputIterator>
std::string decode(InputIterator first, InputIterator last)
{
  static const int INDEX_TABLE[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
  };
  std::string res;
  InputIterator k[4];
  int eq = 0;
  for(; first != last;) {
    for(int i = 1; i <= 4; ++i) {
      k[i-1] = getNext(first, last, INDEX_TABLE);
      if(k[i-1] == last) {
        // If i == 1, input may look like this: "TWFu\n" (i.e.,
        // garbage at the end)
        if(i != 1) {
          res.clear();
        }
        return res;
      } else if(*k[i-1] == '=' && eq == 0) {
        eq = i;
      }
      first = k[i-1]+1;
    }
    if(eq) {
      break;
    }
    int n = (INDEX_TABLE[static_cast<unsigned char>(*k[0])] << 18)+
      (INDEX_TABLE[static_cast<unsigned char>(*k[1])] << 12)+
      (INDEX_TABLE[static_cast<unsigned char>(*k[2])] << 6)+
      INDEX_TABLE[static_cast<unsigned char>(*k[3])];
    res += n >> 16;
    res += n >> 8 & 0xffu;
    res += n & 0xffu;
  }
  if(eq) {
    if(eq <= 2) {
      res.clear();
      return res;
    } else {
      for(int i = eq; i <= 4; ++i) {
        if(*k[i-1] != '=') {
          res.clear();
          return res;
        }
      }
      if(eq == 3) {
        int n = (INDEX_TABLE[static_cast<unsigned char>(*k[0])] << 18)+
          (INDEX_TABLE[static_cast<unsigned char>(*k[1])] << 12);
        res += n >> 16;
      } else if(eq == 4) {
        int n = (INDEX_TABLE[static_cast<unsigned char>(*k[0])] << 18)+
          (INDEX_TABLE[static_cast<unsigned char>(*k[1])] << 12)+
          (INDEX_TABLE[static_cast<unsigned char>(*k[2])] << 6);
        res += n >> 16;
        res += n >> 8 & 0xffu;
      }
    }
  }
  return res;
}

} // namespace base64

} // namespace spdylay

#endif // BASE64_H
