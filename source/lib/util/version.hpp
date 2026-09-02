#pragma once

#include <config/version.hpp>

namespace exl::util {

    namespace impl {
        extern UserVersion s_UserVersion;
        void InitVersion();
    }

    inline UserVersion GetUserVersion() { return impl::s_UserVersion; }
}