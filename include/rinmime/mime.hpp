/* SPDX-License-Identifier: MIT */
#ifndef RINMIME_MIME_HPP
#define RINMIME_MIME_HPP

#include "../../../libc/stdint.h"
#include "../../../libcxx/string.h"
#include "../../../libcxx/vector.h"

#include <stddef.h>

#include "transfer.h"

namespace rinmime {

static constexpr size_t kMaxCStringHeaderNameBytes = 64u * 1024u;

struct Header {
    std::string name;
    std::string value;
};

struct Headers {
    std::vector<Header> values;

    /* name is a compatibility C-string entry point.  Callers needing an
     * arbitrary non-NUL-terminated name should compare Header::name instead. */
    std::string get(const char* name) const;
};

struct Part {
    std::string id;
    std::string type;
    /* True when the message omitted Content-Type and the parser supplied the
     * RFC default.  Consumers may use this bit to apply a shared filename
     * registry without overriding an explicit sender declaration. */
    bool typeWasImplicit = false;
    std::string disposition;
    std::string charset;
    std::string fileName;
    std::vector<uint8_t> body;
};

struct Limits {
    size_t maxRawBytes = RIN_MIME_RAW_MAX;
    size_t maxHeaderBytes = 256u * 1024u;
    size_t maxLineBytes = 64u * 1024u;
    size_t maxNestingDepth = RIN_MIME_NESTING_DEPTH_MAX;
    size_t maxParts = RIN_MIME_PART_COUNT_MAX;
    size_t maxAttachments = RIN_MIME_ATTACHMENT_COUNT_MAX;
    size_t maxBoundaryBytes = 200u;
    size_t maxParameters = 64u;
};

struct Mailbox {
    std::string displayName;
    std::string localPart;
    std::string domain;
    std::string address;
};

struct MailboxLimits {
    size_t maxListBytes = 64u * 1024u;
    size_t maxEntries = 64u;
    size_t maxAddressBytes = 320u;
    size_t maxDisplayBytes = 256u;
};

/* Parse one bounded RFC 5322-style header block. Header names are normalized
 * to lower ASCII and folded lines are joined with one space. The result is
 * failure-atomic and owns its strings. */
bool parseHeaders(const std::string& input, Headers& result,
                  const Limits& limits = Limits{});

/* Parse an envelope mailbox list. This deliberately accepts only the
 * unambiguous dot-atom address form with an optional display name; comments,
 * groups, quoted local-parts, and malformed domain literals are rejected. */
bool parseMailboxList(const std::string& input, std::vector<Mailbox>& result,
                      const MailboxLimits& limits = MailboxLimits{});

/* Parse one complete RFC822/MIME message. The input is never retained. */
bool parseParts(const std::string& raw, std::vector<Part>& parts,
                const Limits& limits = Limits{});

} // namespace rinmime

#endif
