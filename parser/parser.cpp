#include "parser.h"

#include <cctype>
#include <string>

using namespace std;

optional<pair<vector<vector<uint8_t>>, size_t>> parse_resp_command(const vector<uint8_t>& buf, size_t buf_len) {
    if (buf.empty() || buf[0] != '*') {
        return nullopt;
    }

    size_t i = 1;
    auto find_crlf = [&](size_t start) -> optional<size_t> {
        for (size_t j = start; j + 1 < buf_len; ++j) {
            if (buf[j] == '\r' && buf[j + 1] == '\n') {
                return j;
            }
        }

        return nullopt;
    };

    auto crlf_pos_opt = find_crlf(i);
    if (!crlf_pos_opt) {
        return nullopt;
    }

    size_t crlf_pos = *crlf_pos_opt;

    string line(buf.begin() + i, buf.begin() + crlf_pos);
    int num_elems = stoi(line);
    i = crlf_pos + 2;

    vector<vector<uint8_t>> parts;
    for (int j = 0; j < num_elems; ++j) {
        if (i >= buf_len || buf[i] != '$') {
            return nullopt;
        }

        ++i; // skip $

        crlf_pos_opt = find_crlf(i);
        if (!crlf_pos_opt) {
            return nullopt;
        }

        crlf_pos = *crlf_pos_opt;

        line = string(buf.begin() + i, buf.begin() + crlf_pos);
        int len = stoi(line);
        i = crlf_pos + 2;

        if (i + len + 2 > buf_len) {
            return nullopt;
        }

        parts.emplace_back(buf.begin() + i, buf.begin() + i + len);
        i += len + 2;
    }

    return make_pair(parts, i);
}
