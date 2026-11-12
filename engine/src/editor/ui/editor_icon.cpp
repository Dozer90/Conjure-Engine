#include "conjure/editor/ui/editor_icon.h"

#include "editor/ui/editor_icon_data.h"

#include "conjure/platform/window.h"

#include <span>
#include <utility>
#include <vector>

namespace conjure::editor::ui {

    core::result<std::vector<core::image>> editor_icon_images() {
        const std::span<const icon_data::icon_png> files = icon_data::files();
        std::vector<core::image> images = {};
        images.reserve(files.size());
        for (const icon_data::icon_png &file : files) {
            core::result<core::image> decoded = core::image::load_png(std::as_bytes(file.png));
            if (!decoded.has_value()) {
                return core::make_error(
                  core::error_code::PARSE_FAILURE,
                  "editor icon: the {} px PNG does not decode ({})",
                  file.size,
                  decoded.get_error().to_string()
                );
            }
            if (decoded.value().width() != file.size || decoded.value().height() != file.size) {
                return core::make_error(
                  core::error_code::PARSE_FAILURE,
                  "editor icon: the {} px PNG is {} x {}",
                  file.size,
                  decoded.value().width(),
                  decoded.value().height()
                );
            }
            images.push_back(std::move(decoded.value()));
        }
        return images;
    }

    core::result<void> apply_editor_icon(platform::window &window) {
        CONJURE_TRY_ASSIGN(const std::vector<core::image> images, editor_icon_images());
        return window.set_icon(images);
    }
}
