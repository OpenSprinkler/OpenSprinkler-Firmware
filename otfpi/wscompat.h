#ifndef _WS_COMPAT_H_
#define _WS_COMPAT_H_

#include <defines.h>

/**
 * Websocket Compatibility helper functions
*/

#define WSTRIM_DEFAULT " \t\n\r\f\v"

std::string& rtrim(std::string& s, const char* t = WSTRIM_DEFAULT);
std::string* rtrim(std::string *s, const char* t = WSTRIM_DEFAULT);

// trim from beginning of string (left)
std::string& ltrim(std::string& s, const char* t = WSTRIM_DEFAULT);
std::string* ltrim(std::string *s, const char* t = WSTRIM_DEFAULT);

// trim from both ends of string (right then left)
std::string& trim(std::string& s, const char* t = WSTRIM_DEFAULT);
std::string* trim(std::string *s, const char* t = WSTRIM_DEFAULT);

std::string& remove(std::string& s, int index, int count);
std::string* remove(std::string *s, int index, int count);

int indexOf(const std::string& s, const string& item, int from = 0);
int indexOf(const std::string *s, const string& item, int from = 0);
int indexOf(const std::string& s, const char item, int from = 0);
int indexOf(const std::string *s, const char item, int from = 0);

std::string substring(const std::string& s, int start);
std::string substring(const std::string *s, int start);
std::string substring(const std::string& s, int start, int end);
std::string substring(const std::string *s, int start, int end);

uint random(uint max);

bool startsWith(const std::string& s, const std::string& test, int offset = 0);
bool startsWith(const std::string *s, const std::string& test, int offset = 0);

bool equalsIgnoreCase(const std::string& s1, const std::string& s2);
bool equalsIgnoreCase(const std::string *s1, const std::string *s2);

#endif