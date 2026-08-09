// Product-wide identifiers. Nothing else in the codebase should hard-code the
// product name, maker or version. Same convention as the other plug-ins in this
// workspace, so the strings a host lists stay consistent across the line.

#pragma once

namespace fourcolor::productInfo
{
    inline constexpr const char* name    = "FOUR COLOR";

    //  The company is what a host lists under the manufacturer field; the maker
    //  is the person, and the two are not written the same way.
    inline constexpr const char* company = "Naaman";
    inline constexpr const char* maker   = "GUSSA NAAMAN";

    //  1.0.0-rc.1: a release candidate, not the release. It stays -rc until the
    //  owner returns LISTENING, CUBASE MACOS, CUBASE WINDOWS and INSTALLER on a
    //  clean machine, and until signing and notarization exist.
    inline constexpr const char* version = "1.0.0-rc.1";
}
