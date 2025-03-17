/*
 * vsnprintf() is borrowed from pk.
 */

//#include <stdint.h>
//#include <stdarg.h>
#include <stdbool.h>
#include <kernel/types.h>

#include "util/snprintf.h"

/**
 * @brief 将格式化的数据写入一个字符串缓冲区
 *
 * @param out 输出字符串缓冲区，用于存储格式化后的结果
 * @param n 缓冲区大小，防止缓冲区溢出
 * @param s 格式化字符串, 包含普通文本和格式说明符（如 %d、%x 等）
 * @param vl 可变参数列表
 * @return 格式化后的字符串的长度
 */
int32 vsnprintf(char* out, size_t n, const char* s, va_list vl) {
  bool format = false;
  bool longarg = false;
  size_t pos = 0;

  for (; *s; s++) {
    if (format) {
      switch (*s) {
        case 'l':
          longarg = true;
          break;
        case 'p':
          longarg = true;
          if (++pos < n) out[pos - 1] = '0';
          if (++pos < n) out[pos - 1] = 'x';
        case 'x': {
          long num = longarg ? va_arg(vl, long) : va_arg(vl, int);
          for (int i = 2 * (longarg ? sizeof(long) : sizeof(int)) - 1; i >= 0; i--) {
            int d = (num >> (4 * i)) & 0xF;
            if (++pos < n) out[pos - 1] = (d < 10 ? '0' + d : 'a' + d - 10);
          }
          longarg = false;
          format = false;
          break;
        }
        case 'd': {
          long num = longarg ? va_arg(vl, long) : va_arg(vl, int);
          if (num < 0) {
            num = -num;
            if (++pos < n) out[pos - 1] = '-';
          }
          long digits = 1;
          for (long nn = num; nn /= 10; digits++)
            ;
          for (int i = digits - 1; i >= 0; i--) {
            if (pos + i + 1 < n) out[pos + i] = '0' + (num % 10);
            num /= 10;
          }
          pos += digits;
          longarg = false;
          format = false;
          break;
        }
        case 's': {
          const char* s2 = va_arg(vl, const char*);
          while (*s2) {
            if (++pos < n) out[pos - 1] = *s2;
            s2++;
          }
          longarg = false;
          format = false;
          break;
        }
        case 'c': {
          if (++pos < n) out[pos - 1] = (char)va_arg(vl, int);
          longarg = false;
          format = false;
          break;
        }
        default:
          break;
      }
    } else if (*s == '%')
      format = true;
    else if (++pos < n)
      out[pos - 1] = *s;
  }
  if (pos < n)
    out[pos] = 0;
  else if (n)
    out[n - 1] = 0;
  return pos;
}

int32 sprintf(char *out, const char *s, ...) {
	va_list vl;
	va_start(vl, s);
	int32 ret = vsnprintf(out, 0x7fffffff, s, vl);
	va_end(vl);
	return ret;
}