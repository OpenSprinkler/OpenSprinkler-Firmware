#if defined(OSPI)
#include "wscompat.h"
#include <utils.h>

std::string& rtrim(std::string& s, const char* t) {
    s.erase(s.find_last_not_of(t)+1);
    return s;
}

std::string* rtrim(std::string *s, const char* t) {
    s->erase(s->find_last_not_of(t)+1);
    return s;
}

// trim from beginning of string (left)
std::string& ltrim(std::string& s, const char* t) {
    s.erase(0, s.find_first_not_of(t));
    return s;
}

std::string* ltrim(std::string *s, const char* t) {
    s->erase(0, s->find_first_not_of(t));
    return s;
}

// trim from both ends of string (right then left)
std::string& trim(std::string& s, const char* t) {
    return ltrim(rtrim(s, t), t);
}

std::string* trim(std::string *s, const char* t) {
    return ltrim(rtrim(s, t), t);
}

std::string& remove(std::string& s, int index, int count) {
    s.erase(index, count);
    return s;
}

std::string* remove(std::string *s, int index, int count) {
    s->erase(index, count);
    return s;
}

int indexOf(const std::string& s, const std::string& item, int from) {
    return s.find(item, from);
}
int indexOf(const std::string *s, const string& item, int from) {
    return s->find(item, from);
}
int indexOf(const std::string& s, char item, int from) {
    return s.find(item, from);
}
int indexOf(const std::string *s, char item, int from) {
    return s->find(item, from);
}

std::string substring(const std::string& s, int start) {
    return s.substr(start);
}

std::string substring(const std::string *s, int start) {
    return s->substr(start);
}

std::string substring(const std::string& s, int start, int end) {
    return s.substr(start, end-start);
}

std::string substring(const std::string *s, int start, int end) {
    return s->substr(start, end-start);
}

uint random(uint max) {
    return random() % max;
}

bool startsWith(const std::string& s, const std::string& test, int offset) {
    int len = s.length();
    int len2 = test.length();
    if (len < len2) return 0;
	if (offset > len - len2) return 0;
	return strncmp( s.c_str() + offset, test.c_str(), len2 ) == 0;
}

bool startsWith(const std::string *s, const std::string& test, int offset) {
    int len = s->length();
    int len2 = test.length();
    if (len < len2) return 0;
	if (offset > len - len2) return 0;
	return strncmp( s->c_str() + offset, test.c_str(), len2 ) == 0;
}

bool equalsIgnoreCase(const std::string& s1, const std::string& s2) {
    if (&s1 == &s2) return 1;
	if (s1.length() != s2.length()) return 0;
	if (s1.length() == 0) return 1;
	const char *p1 = s1.c_str();
	const char *p2 = s2.c_str();
	while (*p1) {
		if (tolower(*p1++) != tolower(*p2++)) return 0;
	} 
	return 1;
}

bool equalsIgnoreCase(const std::string *s1, const std::string *s2) {
    if (s1 == s2) return 1;
	if (s1->length() != s2->length()) return 0;
	if (s1->length() == 0) return 1;
	const char *p1 = s1->c_str();
	const char *p2 = s2->c_str();
	while (*p1) {
		if (tolower(*p1++) != tolower(*p2++)) return 0;
	} 
	return 1;

}
#endif