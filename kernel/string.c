// See LICENSE for license details.

#include "string.h"

#include <ctype.h>
#include <stdint.h>

void *memcpy(void *dest, const void *src, size_t len) {
  const char *s = src;
  char *d = dest;

  if ((((uintptr_t)dest | (uintptr_t)src) & (sizeof(uintptr_t) - 1)) == 0) {
    while ((void *)d < (dest + len - (sizeof(uintptr_t) - 1))) {
      *(uintptr_t *)d = *(const uintptr_t *)s;
      d += sizeof(uintptr_t);
      s += sizeof(uintptr_t);
    }
  }

  while (d < (char *)(dest + len))
    *d++ = *s++;

  return dest;
}

void *memset(void *dest, int byte, size_t len) {
  if ((((uintptr_t)dest | len) & (sizeof(uintptr_t) - 1)) == 0) {
    uintptr_t word = byte & 0xFF;
    word |= word << 8;
    word |= word << 16;
    word |= word << 16 << 16;

    uintptr_t *d = dest;
    while (d < (uintptr_t *)(dest + len))
      *d++ = word;
  } else {
    char *d = dest;
    while (d < (char *)(dest + len))
      *d++ = byte;
  }
  return dest;
}

size_t strlen(const char *s) {
  const char *p = s;
  while (*p)
    p++;
  return p - s;
}

int strcmp(const char *s1, const char *s2) {
  unsigned char c1, c2;

  do {
    c1 = *s1++;
    c2 = *s2++;
  } while (c1 != 0 && c1 == c2);

  return c1 - c2;
}

char *strcpy(char *dest, const char *src) {
  char *d = dest;
  while ((*d++ = *src++))
    ;
  return dest;
}

char *strchr(const char *p, int ch) {
  char c;
  c = ch;
  for (;; ++p) {
    if (*p == c)
      return ((char *)p);
    if (*p == '\0')
      return (NULL);
  }
}

char *strtok(char *str, const char *delim) {
  static char *current;
  if (str != NULL)
    current = str;
  if (current == NULL)
    return NULL;

  char *start = current;
  while (*start != '\0' && strchr(delim, *start) != NULL)
    start++;

  if (*start == '\0') {
    current = NULL;
    return current;
  }

  char *end = start;
  while (*end != '\0' && strchr(delim, *end) == NULL)
    end++;

  if (*end != '\0') {
    *end = '\0';
    current = end + 1;
  } else
    current = NULL;
  return start;
}

char *strcat(char *dst, const char *src) {
  strcpy(dst + strlen(dst), src);
  return dst;
}

long atol(const char *str) {
  long res = 0;
  int sign = 0;

  while (*str == ' ')
    str++;

  if (*str == '-' || *str == '+') {
    sign = *str == '-';
    str++;
  }

  while (*str) {
    res *= 10;
    res += *str++ - '0';
  }

  return sign ? -res : res;
}

void *memmove(void *dst, const void *src, size_t n) {
  const char *s;
  char *d;

  s = src;
  d = dst;
  if (s < d && s + n > d) {
    s += n;
    d += n;
    while (n-- > 0)
      *--d = *--s;
  } else
    while (n-- > 0)
      *d++ = *s++;

  return dst;
}

// Like strncpy but guaranteed to NUL-terminate.
char *safestrcpy(char *s, const char *t, int n) {
  char *os;

  os = s;
  if (n <= 0)
    return os;
  while (--n > 0 && (*s++ = *t++) != 0)
    ;
  *s = 0;
  return os;
}


/**
 * strncpy - 将源字符串的前n个字符复制到目标字符串
 * @dest: 目标字符串
 * @src: 源字符串
 * @n: 最多复制的字符数
 *
 * 如果src的长度小于n，则用'\0'填充剩余空间
 * 如果src的长度大于等于n，则不会自动添加'\0'
 *
 * 返回值: 目标字符串指针
 */
char *strncpy(char *dest, const char *src, size_t n) {
	size_t i;
	
	// 复制最多n个字符
	for (i = 0; i < n && src[i] != '\0'; i++) {
			dest[i] = src[i];
	}
	
	// 如果源字符串长度小于n，用'\0'填充剩余空间
	for (; i < n; i++) {
			dest[i] = '\0';
	}
	
	return dest;
}

/**
 * strncmp - 比较两个字符串的前n个字符
 * @p: 字符串1
 * @q: 字符串2
 * @n: 比较的字符数
 * 
 * 返回值: 0表示相等，非0表示不等
 */
int strncmp(const char *p, const char *q, size_t n) {
	while (n > 0 && *p && *p == *q) {
		n--, p++, q++;
	}
	if (n == 0 || (*p == 0 && *q == 0)) {
		return 0;
	}
	return (char)*p - (char)*q;
}