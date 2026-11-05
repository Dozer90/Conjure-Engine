#include "conjure/core/dynamic_library.h"

#include <utility>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

dynamic_library::dynamic_library() {}

dynamic_library::~dynamic_library() {
    unload();
}

dynamic_library::dynamic_library(dynamic_library &&other) noexcept :
    mHandle(std::exchange(other.mHandle, nullptr)),
    mPath(std::move(other.mPath)) {
    other.mPath.clear();
}

dynamic_library &dynamic_library::operator=(dynamic_library &&other) noexcept {
    if (this != &other) {
        unload();
        mHandle = std::exchange(other.mHandle, nullptr);
        mPath = std::move(other.mPath);
        other.mPath.clear();
    }
    return *this;
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods (load, unload, symbol, extension and prefix live in dynamic_library_<platform>.cpp)

std::string dynamic_library::decorate_name(std::string_view baseName) {
    if (baseName.empty()) {
        return {};
    }
    if (baseName.ends_with(extension())) {
        return std::string(baseName);
    }

    // The prefix belongs to the file name, not to the directory part: "modules/conjure_foo" is
    // "modules/libconjure_foo.so", never "libmodules/conjure_foo.so".
    const std::size_t lastSeparator = baseName.find_last_of("/\\");
    const std::size_t fileNameStart = lastSeparator == std::string_view::npos ? 0 : lastSeparator + 1;

    std::string decorated = {};
    decorated.reserve(prefix().size() + baseName.size() + extension().size());
    decorated.append(baseName.substr(0, fileNameStart));
    decorated.append(prefix());
    decorated.append(baseName.substr(fileNameStart));
    decorated.append(extension());
    return decorated;
}
