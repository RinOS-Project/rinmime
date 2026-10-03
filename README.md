# RinMIME

RinMIME provides bounded MIME parsing and transfer decoding for RinOS mail and content-handling components.

## Public API contract

| Requirement | Contract |
| --- | --- |
| Purpose | RinMIME provides bounded MIME parsing and transfer decoding for RinOS mail and content-handling components. |
| Supported API | Public interfaces are `rinmime/mime.hpp`, `rinmime/registry.h`, `rinmime/registry.hpp`, and `rinmime/transfer.h`. |
| Unsupported API | It does not fetch external resources, render HTML, authorize attachments, or promise support for every MIME extension or transfer encoding. |
| ownership | Input bytes remain caller-owned. C++ parse results own their represented data as described by their types; callers must respect returned spans and lifetimes. |
| thread-safety | Independent parser instances may be used concurrently. Shared registry mutation and consumer-owned callbacks require caller synchronization. |
| limits | Nesting depth is capped at 16, parts at 256, attachments at 64, raw input at 32 MiB, decoded part data at 64 MiB, filenames at 255 bytes, registry path lookup at 4 KiB, and boundary candidate checks at 64 MiB by default. Counted registry paths reject embedded NUL bytes. Callers can lower `Limits::maxScanBytes` for a stricter parser budget. |
| errors | Malformed, unsupported, or over-limit input is reported as parse/decode failure. In exception-enabled C++ builds, allocation failures at the public parser/registry boundary are also mapped to `false` or an empty result; no partially parsed data is published. Do not use failed results as trusted content. |
| ABI stability | C declarations in the public headers define the C ABI; C++ interfaces have compiler-specific ABI. No cross-version ABI guarantee is published. |
| security | Treat messages and attachment names as untrusted. Enforce authorization and safe filesystem handling in the caller; parsing does not make content safe to render or store. |
| build | No standalone build manifest is documented. Integrate the public headers and sources through the RinOS consumer build. |
| test | A `tests` directory is present, but this repository does not document a standalone test command. Run its tests through the owning RinOS build when available. |

`rinmime::parseMailboxList` keeps ASCII dot-atom local-parts as its default.
SMTPUTF8 callers can set `MailboxLimits::allowUtf8LocalPart` to accept bounded,
strictly decoded UTF-8 local-parts. Domain labels remain ASCII; the parser
does not validate punycode A-labels or perform IDNA conversion/normalization.
