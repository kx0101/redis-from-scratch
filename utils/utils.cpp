#include "utils.h"

#include <algorithm>

using namespace std;

string bytes_to_string(const vector<uint8_t>& bytes) {
    return string(bytes.begin(), bytes.end());
}

string to_upper(string s) {
    transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}
