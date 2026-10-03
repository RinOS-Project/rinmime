/* SPDX-License-Identifier: MIT */
#include "../include/rinmime/mime.hpp"

namespace rinmime {
namespace {

size_t cStringLength(const char* value) {
    size_t length = 0u;
    if (value == nullptr) return 0u;
    while (value[length] != '\0') ++length;
    return length;
}

std::string decimal(unsigned value) {
    std::string result;
    do {
        result.insert(result.begin(), static_cast<char>('0' + value % 10u));
        value /= 10u;
    } while (value != 0u);
    return result;
}

char asciiLower(char value) {
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value + ('a' - 'A')) : value;
}

bool asciiSpace(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

std::string trim(const std::string& value) {
    size_t begin = 0u;
    size_t end = value.size();
    while (begin < end && asciiSpace(static_cast<unsigned char>(value[begin]))) ++begin;
    while (end > begin && asciiSpace(static_cast<unsigned char>(value[end - 1u]))) --end;
    return value.substr(begin, end - begin);
}

std::string lower(std::string value) {
    for (char& byte : value) byte = asciiLower(byte);
    return value;
}

bool contentTypeConsistent(const Headers& headers) {
    std::string first;
    bool found = false;
    for (const auto& item : headers.values) {
        if (item.name != "content-type") continue;
        const std::string value = lower(trim(item.value));
        if (!found) {
            first = value;
            found = true;
        } else if (value != first) {
            return false;
        }
    }
    return true;
}

bool startsWithInsensitive(const std::string& value, const char* prefix) {
    size_t count = cStringLength(prefix);
    if (value.size() < count) return false;
    for (size_t index = 0u; index < count; ++index)
        if (asciiLower(value[index]) != asciiLower(prefix[index])) return false;
    return true;
}

struct ScanBudget {
    size_t remaining;

    bool consume(size_t amount) {
        if (amount > remaining) return false;
        remaining -= amount;
        return true;
    }
};

size_t findMarker(const std::string& body, const std::string& marker,
                  size_t from, ScanBudget& budget) {
    const size_t markerLast = marker.size() - 1u;
    const size_t lastCandidate = body.size() - marker.size();
    size_t cursor = from;
    while (cursor <= lastCandidate) {
        const char tail = body[cursor + markerLast];
        size_t shift = marker.size();
        for (size_t index = markerLast; index != 0u; --index) {
            if (marker[index - 1u] == tail) {
                shift = markerLast - (index - 1u);
                break;
            }
        }
        if (tail == marker[markerLast]) {
            if (!budget.consume(marker.size())) return std::string::npos;
            size_t index = 0u;
            while (index < markerLast &&
                   body[cursor + index] == marker[index])
                ++index;
            if (index == markerLast) return cursor;
        }
        if (!budget.consume(shift)) return std::string::npos;
        cursor += shift;
    }
    return std::string::npos;
}

bool headerNameValid(const std::string& name) {
    if (name.empty()) return false;
    for (char byte : name) {
        const unsigned char value = static_cast<unsigned char>(byte);
        if (!((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
              (value >= '0' && value <= '9') || value == '!' || value == '#' ||
              value == '$' || value == '%' || value == '&' || value == '\'' ||
              value == '*' || value == '+' || value == '-' || value == '.' ||
              value == '^' || value == '_' || value == '`' || value == '|' ||
              value == '~')) return false;
    }
    return true;
}

bool headerValueValid(const std::string& value, size_t maxLineBytes) {
    if (value.size() > maxLineBytes) return false;
    for (char byte : value) {
        const unsigned char valueByte = static_cast<unsigned char>(byte);
        if (valueByte == 0u || valueByte == 0x7fu ||
            (valueByte < 0x20u && valueByte != '\t')) return false;
    }
    return true;
}

bool parseHeaderBlock(const std::string& input, Headers& result,
                      const Limits& limits) {
    size_t cursor = 0u;
    result.values.clear();
    if (input.size() > limits.maxHeaderBytes) return false;
    while (cursor < input.size()) {
        size_t end = input.find('\n', cursor);
        if (end == std::string::npos) end = input.size();
        if (end - cursor > limits.maxLineBytes) return false;
        std::string line = input.substr(cursor, end - cursor);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        cursor = end == input.size() ? end : end + 1u;
        if (line.empty()) return true;
        if ((line[0] == ' ' || line[0] == '\t') && !result.values.empty()) {
            std::string continuation = trim(line);
            if (!headerValueValid(continuation, limits.maxLineBytes)) return false;
            result.values.back().value += " " + continuation;
            if (!headerValueValid(result.values.back().value, limits.maxLineBytes)) return false;
            continue;
        }
        const size_t colon = line.find(':');
        if (colon == std::string::npos) return false;
        std::string name = trim(line.substr(0u, colon));
        std::string value = trim(line.substr(colon + 1u));
        if (!headerNameValid(name) || !headerValueValid(value, limits.maxLineBytes) ||
            result.values.size() >= limits.maxParts) return false;
        result.values.push_back({lower(name), value});
    }
    return true;
}

std::string parameter(const std::string& value, const char* wanted,
                      size_t maxParameters, bool* valid = nullptr) {
    if (valid != nullptr) *valid = true;
    const std::string name = lower(wanted);
    size_t cursor = 0u;
    size_t parameterCount = 0u;
    bool found = false;
    std::string selected;
    while ((cursor = value.find(';', cursor)) != std::string::npos) {
        if (parameterCount++ >= maxParameters) {
            if (valid != nullptr) *valid = false;
            return {};
        }
        ++cursor;
        while (cursor < value.size() && asciiSpace(static_cast<unsigned char>(value[cursor]))) ++cursor;
        const size_t keyStart = cursor;
        while (cursor < value.size() &&
               headerNameValid(value.substr(cursor, 1u)))
            ++cursor;
        if (cursor == keyStart) {
            if (valid != nullptr) *valid = false;
            return {};
        }
        const std::string key = lower(value.substr(keyStart, cursor - keyStart));
        while (cursor < value.size() && asciiSpace(static_cast<unsigned char>(value[cursor]))) ++cursor;
        if (cursor >= value.size() || value[cursor] != '=') {
            if (valid != nullptr) *valid = false;
            return {};
        }
        ++cursor;
        while (cursor < value.size() && asciiSpace(static_cast<unsigned char>(value[cursor]))) ++cursor;
        std::string result;
        if (cursor < value.size() && value[cursor] == '"') {
            ++cursor;
            bool closed = false;
            while (cursor < value.size()) {
                const unsigned char byte = static_cast<unsigned char>(value[cursor++]);
                if (byte == '"') {
                    closed = true;
                    break;
                }
                if (byte == '\\') {
                    if (cursor >= value.size()) break;
                    const unsigned char escaped = static_cast<unsigned char>(value[cursor++]);
                    if (escaped < 0x20u && escaped != '\t') break;
                    if (escaped == 0x7fu) break;
                    result.push_back(static_cast<char>(escaped));
                    continue;
                }
                if (byte < 0x20u && byte != '\t') break;
                if (byte == 0x7fu) break;
                result.push_back(static_cast<char>(byte));
            }
            if (!closed) {
                if (valid != nullptr) *valid = false;
                return {};
            }
            while (cursor < value.size() && asciiSpace(static_cast<unsigned char>(value[cursor]))) ++cursor;
            if (cursor < value.size() && value[cursor] != ';') {
                if (valid != nullptr) *valid = false;
                return {};
            }
        } else {
            const size_t start = cursor;
            while (cursor < value.size() && value[cursor] != ';') ++cursor;
            result = trim(value.substr(start, cursor - start));
            if (result.empty()) {
                if (valid != nullptr) *valid = false;
                return {};
            }
            for (unsigned char byte : result) {
                if (byte <= 0x20u || byte == 0x7fu) {
                    if (valid != nullptr) *valid = false;
                    return {};
                }
            }
        }
        if (!found && (key == name || key == name + "*")) {
            selected = result;
            found = true;
        }
    }
    return found ? selected : std::string{};
}

bool findBoundary(const std::string& body, const std::string& marker,
                  size_t from, size_t& position, bool& closing,
                  ScanBudget& budget) {
    if (marker.empty() || from >= body.size() || marker.size() > body.size() - from)
        return false;
    size_t cursor = from;
    const size_t lastCandidate = body.size() - marker.size();
    while (cursor <= lastCandidate) {
        const size_t candidate = findMarker(body, marker, cursor, budget);
        if (candidate == std::string::npos || candidate > lastCandidate) {
            if (body.size() - cursor > budget.remaining)
                return false;
            budget.remaining -= body.size() - cursor;
            return false;
        }
        cursor = candidate;
        const size_t after = cursor + marker.size();
        const bool atLineStart = cursor == 0u || body[cursor - 1u] == '\n';
        const bool validSuffix = after == body.size() || body.compare(after, 2u, "--") == 0 ||
            body.compare(after, 2u, "\r\n") == 0 ||
            (after < body.size() && body[after] == '\n');
        if (atLineStart && validSuffix) {
            position = cursor;
            closing = body.compare(after, 2u, "--") == 0;
            return true;
        }
        ++cursor;
    }
    return false;
}

bool splitMime(const Headers& headers, const std::string& body,
               const std::string& prefix, size_t depth,
               std::vector<Part>& parts, const Limits& limits,
               ScanBudget& scanBudget) {
    if (depth >= limits.maxNestingDepth || parts.size() >= limits.maxParts) return false;
    if (!contentTypeConsistent(headers)) return false;
    std::string contentType = headers.get("content-type");
    const bool typeWasImplicit = contentType.empty();
    if (typeWasImplicit) contentType = "text/plain";
    const size_t semicolon = contentType.find(';');
    std::string type = lower(trim(contentType.substr(0u, semicolon)));
    if (startsWithInsensitive(type, "multipart/")) {
        bool parametersValid = true;
        const std::string boundary = parameter(contentType, "boundary", limits.maxParameters,
                                               &parametersValid);
        if (!parametersValid || boundary.empty() || boundary.size() > limits.maxBoundaryBytes)
            return false;
        const std::string marker = "--" + boundary;
        size_t cursor = 0u;
        unsigned child = 1u;
        bool terminated = false;
        size_t position = 0u;
        bool closing = false;
        while (findBoundary(body, marker, cursor, position, closing, scanBudget)) {
            size_t after = position + marker.size();
            if (closing) { terminated = true; break; }
            if (body.compare(after, 2u, "\r\n") == 0) after += 2u;
            else if (after < body.size() && body[after] == '\n') ++after;
            size_t next = 0u;
            bool nextClosing = false;
            if (!findBoundary(body, marker, after, next, nextClosing, scanBudget)) return false;
            (void)nextClosing;
            std::string raw = body.substr(after, next - after);
            while (!raw.empty() && (raw.back() == '\r' || raw.back() == '\n')) raw.pop_back();
            size_t separator = raw.find("\r\n\r\n");
            size_t skip = 4u;
            if (separator == std::string::npos) { separator = raw.find("\n\n"); skip = 2u; }
            if (separator == std::string::npos) return false;
            Headers childHeaders;
            if (!parseHeaderBlock(raw.substr(0u, separator), childHeaders, limits)) return false;
            const std::string number = decimal(child++);
            const std::string childId = prefix.empty() ? number : prefix + "." + number;
            if (!splitMime(childHeaders, raw.substr(separator + skip), childId,
                           depth + 1u, parts, limits, scanBudget)) return false;
            cursor = next;
        }
        return terminated;
    }
    Part part;
    part.id = prefix.empty() ? "1" : prefix;
    part.type = type;
    part.typeWasImplicit = typeWasImplicit;
    part.disposition = lower(trim(headers.get("content-disposition")));
    bool dispositionParametersValid = true;
    part.fileName = parameter(headers.get("content-disposition"), "filename",
                              limits.maxParameters, &dispositionParametersValid);
    if (!dispositionParametersValid) return false;
    bool contentTypeParametersValid = true;
    if (part.fileName.empty())
        part.fileName = parameter(contentType, "name", limits.maxParameters,
                                  &contentTypeParametersValid);
    if (!contentTypeParametersValid) return false;
    bool charsetParametersValid = true;
    part.charset = parameter(contentType, "charset", limits.maxParameters,
                             &charsetParametersValid);
    if (!charsetParametersValid) return false;
    const bool isAttachment = startsWithInsensitive(part.disposition, "attachment") ||
                              !part.fileName.empty();
    if (isAttachment && part.fileName.empty()) part.fileName = "attachment";
    if (isAttachment && !rin_mime_filename_valid(part.fileName.data(), part.fileName.size()))
        part.fileName = "attachment";
    const std::string encoding = headers.get("content-transfer-encoding");
    if (body.size() > RIN_MIME_DECODED_PART_MAX) return false;
    part.body.assign(body.size(), 0u);
    size_t decodedSize = 0u;
    const int result = rin_mime_decode_transfer(
        encoding.data(), encoding.size(), reinterpret_cast<const uint8_t*>(body.data()), body.size(),
        part.body.empty() ? nullptr : part.body.data(), part.body.size(), &decodedSize);
    if (result != RIN_MIME_OK) return false;
    part.body.resize(decodedSize);
    if (isAttachment) {
        size_t attachmentCount = 0u;
        for (const auto& existing : parts) if (!existing.fileName.empty()) ++attachmentCount;
        if (attachmentCount >= limits.maxAttachments) return false;
    }
    parts.push_back(std::move(part));
    return true;
}

bool mailboxAtext(unsigned char value) {
    return (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9') || value == '!' || value == '#' ||
           value == '$' || value == '%' || value == '&' || value == '\'' ||
           value == '*' || value == '+' || value == '-' || value == '/' ||
           value == '=' || value == '?' || value == '^' || value == '_' ||
           value == '`' || value == '{' || value == '|' || value == '}' ||
           value == '~';
}

bool mailboxIpv4AddressValid(const std::string& address) {
    size_t start = 0u;
    size_t octets = 0u;
    for (size_t index = 0u; index <= address.size(); ++index) {
        if (index != address.size() && address[index] != '.') continue;
        const size_t length = index - start;
        if (length == 0u || length > 3u || octets >= 4u ||
            (length > 1u && address[start] == '0'))
            return false;
        unsigned value = 0u;
        for (size_t digit = start; digit < index; ++digit) {
            if (address[digit] < '0' || address[digit] > '9') return false;
            value = value * 10u +
                static_cast<unsigned>(address[digit] - '0');
        }
        if (value > 255u) return false;
        ++octets;
        start = index + 1u;
    }
    return octets == 4u;
}

bool mailboxIpv6AddressValid(const std::string& address) {
    if (address.empty()) return false;
    bool compressed = false;
    size_t words = 0u;
    size_t cursor = 0u;
    if (address.size() >= 2u && address[0] == ':' && address[1] == ':') {
        compressed = true;
        cursor = 2u;
        if (cursor == address.size()) return true;
    }
    while (cursor < address.size()) {
        size_t end = address.find(':', cursor);
        if (end == std::string::npos) end = address.size();
        if (end == cursor) return false;
        const std::string token = address.substr(cursor, end - cursor);
        if (token.find('.') != std::string::npos) {
            if (end != address.size() || !mailboxIpv4AddressValid(token))
                return false;
            words += 2u;
            cursor = end;
            break;
        }
        if (token.size() > 4u) return false;
        for (unsigned char value : token)
            if (!((value >= '0' && value <= '9') ||
                  (value >= 'a' && value <= 'f') ||
                  (value >= 'A' && value <= 'F')))
                return false;
        ++words;
        cursor = end;
        if (cursor == address.size()) break;
        ++cursor;
        if (cursor == address.size()) return false;
        if (address[cursor] == ':') {
            if (compressed) return false;
            compressed = true;
            ++cursor;
            if (cursor == address.size()) break;
        }
    }
    return compressed ? words < 8u : words == 8u;
}

bool mailboxDomainLiteralValid(const std::string& domain) {
    if (domain.size() < 3u || domain.front() != '[' || domain.back() != ']')
        return false;
    const std::string literal = domain.substr(1u, domain.size() - 2u);
    if (literal.size() >= 5u &&
        (literal[0] == 'I' || literal[0] == 'i') &&
        (literal[1] == 'P' || literal[1] == 'p') &&
        (literal[2] == 'V' || literal[2] == 'v') && literal[3] == '6' &&
        literal[4] == ':')
        return mailboxIpv6AddressValid(literal.substr(5u));
    return mailboxIpv4AddressValid(literal);
}

bool mailboxDomainValid(const std::string& domain) {
    if (domain.empty() || domain.size() > 255u) return false;
    if (domain.front() == '[' || domain.back() == ']')
        return mailboxDomainLiteralValid(domain);
    size_t labelStart = 0u;
    for (size_t index = 0u; index <= domain.size(); ++index) {
        if (index != domain.size() && domain[index] != '.') continue;
        const size_t labelSize = index - labelStart;
        if (labelSize == 0u || labelSize > 63u ||
            domain[labelStart] == '-' || domain[index - 1u] == '-')
            return false;
        for (size_t label = labelStart; label < index; ++label) {
            const unsigned char value = static_cast<unsigned char>(domain[label]);
            if (!((value >= 'A' && value <= 'Z') ||
                  (value >= 'a' && value <= 'z') ||
                  (value >= '0' && value <= '9') || value == '-'))
                return false;
        }
        labelStart = index + 1u;
    }
    return true;
}

bool mailboxUtf8Scalar(const std::string& input, size_t offset,
                       uint32_t& codepoint, size_t& byteCount) {
    if (offset >= input.size()) return false;
    const unsigned char first = static_cast<unsigned char>(input[offset]);
    if (first < 0x80u) {
        codepoint = first;
        byteCount = 1u;
        return true;
    }
    size_t continuation = 0u;
    uint32_t value = 0u;
    if (first >= 0xc2u && first <= 0xdfu) {
        continuation = 1u;
        value = first & 0x1fu;
    } else if (first >= 0xe0u && first <= 0xefu) {
        continuation = 2u;
        value = first & 0x0fu;
    } else if (first >= 0xf0u && first <= 0xf4u) {
        continuation = 3u;
        value = first & 0x07u;
    } else {
        return false;
    }
    if (continuation > input.size() - offset - 1u) return false;
    for (size_t index = 1u; index <= continuation; ++index) {
        const unsigned char next =
            static_cast<unsigned char>(input[offset + index]);
        if ((next & 0xc0u) != 0x80u) return false;
        value = (value << 6) | (next & 0x3fu);
    }
    if ((continuation == 1u && value < 0x80u) ||
        (continuation == 2u && value < 0x800u) ||
        (continuation == 3u && value < 0x10000u) ||
        (value >= 0xd800u && value <= 0xdfffu) || value > 0x10ffffu)
        return false;
    codepoint = value;
    byteCount = continuation + 1u;
    return true;
}

bool mailboxUtf8LocalScalarAllowed(uint32_t value) {
    if (value <= 0xa0u || value == 0x1680u || value == 0x180eu ||
        value == 0x205fu || value == 0x3000u || value == 0x00adu ||
        value == 0x034fu ||
        value == 0x061cu || (value >= 0x2000u && value <= 0x200fu) ||
        (value >= 0x2028u && value <= 0x202fu) ||
        (value >= 0x2060u && value <= 0x206fu) || value == 0xfeffu ||
        (value >= 0xfff9u && value <= 0xfffbu) ||
        (value & 0xfffeu) == 0xfffeu ||
        (value >= 0xfdd0u && value <= 0xfdefu))
        return false;
    return true;
}

bool mailboxQuotedPairAsciiAllowed(unsigned char value) {
    return value == '\t' || (value >= 0x20u && value <= 0x7eu);
}

bool mailboxFindAddressAt(const std::string& address, size_t& at) {
    bool quoted = false;
    bool escaped = false;
    at = std::string::npos;
    for (size_t index = 0u; index < address.size(); ++index) {
        const char value = address[index];
        if (quoted) {
            if (escaped) escaped = false;
            else if (value == '\\') escaped = true;
            else if (value == '"') quoted = false;
            continue;
        }
        if (value == '"') {
            quoted = true;
        } else if (value == '@') {
            if (at != std::string::npos) return false;
            at = index;
        }
    }
    return !quoted && !escaped && at != std::string::npos;
}

bool mailboxQuotedLocalValid(const std::string& local, bool allowUtf8) {
    if (local.size() < 2u || local.front() != '"' || local.back() != '"')
        return false;
    for (size_t index = 1u; index + 1u < local.size(); ++index) {
        const unsigned char value = static_cast<unsigned char>(local[index]);
        if (value == '\\') {
            if (++index + 1u >= local.size()) return false;
            const unsigned char escaped = static_cast<unsigned char>(local[index]);
            if (!mailboxQuotedPairAsciiAllowed(escaped)) return false;
            continue;
        }
        if (value < 0x80u) {
            if (value < 0x20u || value > 0x7eu || value == '"') return false;
            continue;
        }
        if (!allowUtf8) return false;
        uint32_t codepoint = 0u;
        size_t byteCount = 0u;
        if (!mailboxUtf8Scalar(local, index, codepoint, byteCount) ||
            byteCount > local.size() - 1u - index ||
            !mailboxUtf8LocalScalarAllowed(codepoint))
            return false;
        index += byteCount - 1u;
    }
    return true;
}

bool mailboxAddressParse(const std::string& address, Mailbox& output,
                         const MailboxLimits& limits) {
    size_t at = std::string::npos;
    if (!mailboxFindAddressAt(address, at) || at == 0u ||
        at + 1u >= address.size() ||
        address.size() > limits.maxAddressBytes)
        return false;
    const std::string local = address.substr(0u, at);
    const std::string domain = address.substr(at + 1u);
    if (local.size() > 64u || local.front() == '.' || local.back() == '.')
        return false;
    if (local.front() == '"') {
        if (!mailboxQuotedLocalValid(local, limits.allowUtf8LocalPart))
            return false;
    } else {
        for (size_t index = 0u; index < local.size();) {
            const unsigned char value = static_cast<unsigned char>(local[index]);
            if (local[index] == '.' && index != 0u && local[index - 1u] == '.')
                return false;
            if (value < 0x80u) {
                if (local[index] != '.' && !mailboxAtext(value)) return false;
                ++index;
                continue;
            }
            if (!limits.allowUtf8LocalPart) return false;
            uint32_t codepoint = 0u;
            size_t byteCount = 0u;
            if (!mailboxUtf8Scalar(local, index, codepoint, byteCount) ||
                !mailboxUtf8LocalScalarAllowed(codepoint))
                return false;
            index += byteCount;
        }
    }
    if (!mailboxDomainValid(domain)) return false;
    output.localPart = local;
    output.domain = domain;
    output.address = address;
    return true;
}

bool mailboxDisplayParse(const std::string& source, std::string& output) {
    const std::string display = trim(source);
    if (display.empty()) {
        output.clear();
        return true;
    }
    if (display.front() == '"') {
        if (display.size() < 2u || display.back() != '"') return false;
        output.clear();
        for (size_t index = 1u; index + 1u < display.size(); ++index) {
            const unsigned char value = static_cast<unsigned char>(display[index]);
            if (value == '\\') {
                if (++index + 1u >= display.size()) return false;
                const unsigned char escaped =
                    static_cast<unsigned char>(display[index]);
                if (escaped < 0x20u || escaped == 0x7fu) return false;
                output.push_back(display[index]);
            } else {
                if (value < 0x20u || value == 0x7fu || value == '"') return false;
                output.push_back(display[index]);
            }
        }
        return true;
    }
    if (display.find('"') != std::string::npos ||
        display.find('<') != std::string::npos ||
        display.find('>') != std::string::npos ||
        display.find(',') != std::string::npos ||
        display.find(':') != std::string::npos)
        return false;
    for (unsigned char value : display)
        if (value < 0x20u || value == 0x7fu) return false;
    output = display;
    return true;
}

bool mailboxAngleBounds(const std::string& item, size_t& left, size_t& right) {
    bool quoted = false;
    bool escaped = false;
    left = right = std::string::npos;
    for (size_t index = 0u; index < item.size(); ++index) {
        const char value = item[index];
        if (quoted) {
            if (escaped) escaped = false;
            else if (value == '\\') escaped = true;
            else if (value == '"') quoted = false;
            continue;
        }
        if (value == '"') quoted = true;
        else if (value == '<') {
            if (left != std::string::npos || right != std::string::npos)
                return false;
            left = index;
        } else if (value == '>') {
            if (left == std::string::npos || right != std::string::npos)
                return false;
            right = index;
        }
    }
    return !quoted && !escaped &&
        ((left == std::string::npos && right == std::string::npos) ||
         (left != std::string::npos && right != std::string::npos && right > left));
}

bool mailboxItemParse(const std::string& source, Mailbox& output,
                      const MailboxLimits& limits) {
    const std::string item = trim(source);
    if (item.empty()) return false;
    size_t left = std::string::npos;
    size_t right = std::string::npos;
    if (!mailboxAngleBounds(item, left, right)) return false;
    std::string address;
    if (left == std::string::npos && right == std::string::npos) {
        output.displayName.clear();
        address = item;
    } else {
        if (left == 0u || right + 1u != item.size())
            return false;
        if (!mailboxDisplayParse(item.substr(0u, left), output.displayName))
            return false;
        address = trim(item.substr(left + 1u, right - left - 1u));
    }
    return output.displayName.size() <= limits.maxDisplayBytes &&
           mailboxAddressParse(address, output, limits);
}

bool mailboxListScan(const std::string& input, std::vector<Mailbox>& output,
                     const MailboxLimits& limits) {
    if (input.empty() || input.size() > limits.maxListBytes ||
        limits.maxEntries == 0u) return false;
    std::vector<Mailbox> candidate;
    size_t start = 0u;
    bool quoted = false;
    bool escaped = false;
    bool angle = false;
    for (size_t index = 0u; index <= input.size(); ++index) {
        const bool end = index == input.size();
        const char value = end ? ',' : input[index];
        if (!end) {
            if (quoted) {
                if (escaped) escaped = false;
                else if (value == '\\') escaped = true;
                else if (value == '"') quoted = false;
                continue;
            }
            if (value == '"') {
                quoted = true;
                continue;
            }
            if (value == '<') {
                if (angle) return false;
                angle = true;
                continue;
            }
            if (value == '>') {
                if (!angle) return false;
                angle = false;
                continue;
            }
        }
        if (value != ',' || quoted || angle) continue;
        if (candidate.size() >= limits.maxEntries ||
            !mailboxItemParse(input.substr(start, index - start),
                               candidate.emplace_back(), limits))
            return false;
        start = index + 1u;
    }
    if (quoted || escaped || angle || candidate.empty()) return false;
    output.swap(candidate);
    return true;
}

} // namespace

std::string Headers::get(const char* name) const {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    try {
#endif
    if (name == nullptr) return {};
    std::string wanted;
    size_t length = 0u;
    while (length < kMaxCStringHeaderNameBytes && name[length] != '\0')
        ++length;
    if (length == kMaxCStringHeaderNameBytes) return {};
    wanted.reserve(length);
    for (size_t index = 0u; index < length; ++index)
        wanted.push_back(asciiLower(name[index]));
    for (const Header& item : values)
        if (item.name == wanted) return item.value;
    return {};
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    } catch (const std::bad_alloc&) {
        return {};
    }
#endif
}

bool parseHeaders(const std::string& input, Headers& result,
                  const Limits& limits) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    try {
#endif
    Headers candidate;
    if (!parseHeaderBlock(input, candidate, limits)) return false;
    result = std::move(candidate);
    return true;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    } catch (const std::bad_alloc&) {
        return false;
    }
#endif
}

bool parseMailboxList(const std::string& input, std::vector<Mailbox>& result,
                      const MailboxLimits& limits) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    try {
#endif
    std::vector<Mailbox> candidate;
    if (!mailboxListScan(input, candidate, limits)) return false;
    result.swap(candidate);
    return true;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    } catch (const std::bad_alloc&) {
        return false;
    }
#endif
}

bool parseParts(const std::string& raw, std::vector<Part>& parts, const Limits& limits) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    try {
#endif
    parts.clear();
    std::vector<Part> candidate;
    if (raw.empty() || raw.size() > limits.maxRawBytes) return false;
    size_t separator = raw.find("\r\n\r\n");
    size_t skip = 4u;
    if (separator == std::string::npos) { separator = raw.find("\n\n"); skip = 2u; }
    if (separator == std::string::npos) return false;
    Headers root;
    if (!parseHeaderBlock(raw.substr(0u, separator), root, limits)) return false;
    ScanBudget scanBudget{limits.maxScanBytes};
    if (!splitMime(root, raw.substr(separator + skip), {}, 0u, candidate, limits,
                   scanBudget)) return false;
    parts.swap(candidate);
    return true;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    } catch (const std::bad_alloc&) {
        return false;
    }
#endif
}

} // namespace rinmime
