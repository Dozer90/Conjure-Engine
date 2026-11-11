#include "conjure/editor/ui/editor_panel.h"

#include "editor/ui/ui_support.h"

#include <algorithm>
#include <utility>

namespace conjure::editor::ui {

    namespace {
        /// <summary>
        /// A panel over a module's panel_desc: the draw callback receives the ImGui context.
        /// </summary>
        class module_panel final : public editor_panel {
            private:
                panel_desc mDesc = {};

            public:
                explicit module_panel(const panel_desc &desc) :
                    editor_panel(desc.title != nullptr ? desc.title : "Module panel", desc.openByDefault),
                    mDesc(desc) {}

                ~module_panel() override = default;

            public:
                void draw_contents(editor_context &context) override {
                    (void) context;
                    if (mDesc.draw != nullptr) {
                        mDesc.draw(ImGui::GetCurrentContext(), mDesc.userData);
                    }
                }
        };
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // editor_panel

    editor_panel::editor_panel(std::string title, bool open) :
        mTitle(std::move(title)),
        mOpen(open) {}

    editor_panel::~editor_panel() = default;

    void editor_panel::draw(editor_context &context) {
        if (!mOpen || !support::frame_is_open()) {
            return;
        }
        if (ImGui::Begin(mTitle.c_str(), &mOpen)) {
            draw_contents(context);
        }
        ImGui::End();
    }

    void editor_panel::update(editor_context &context) {
        (void) context;
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // panel_registry

    panel_registry::panel_registry() = default;
    panel_registry::~panel_registry() = default;

    void panel_registry::add(panel_descriptor descriptor) {
        if (descriptor.name.empty() || !descriptor.create) {
            return;
        }
        const auto existing = std::find_if(mDescriptors.begin(), mDescriptors.end(), [&descriptor](
          const panel_descriptor &entry
        ) {
            return entry.name == descriptor.name;
        });
        if (existing != mDescriptors.end()) {
            *existing = std::move(descriptor);
            return;
        }
        mDescriptors.push_back(std::move(descriptor));
    }

    void panel_registry::add_module_panel(std::string_view owner, const panel_desc &desc) {
        if (desc.title == nullptr || desc.draw == nullptr) {
            return;
        }
        panel_descriptor descriptor = {};
        descriptor.name = std::string(owner) + "." + desc.title;
        descriptor.title = desc.title;
        descriptor.owner = std::string(owner);
        descriptor.openByDefault = desc.openByDefault;
        const panel_desc copy = desc;
        descriptor.create = [copy]() -> std::unique_ptr<editor_panel> {
            return std::make_unique<module_panel>(copy);
        };
        add(std::move(descriptor));
    }

    usize panel_registry::remove_owned_by(std::string_view owner) {
        const usize before = mDescriptors.size();
        std::erase_if(mDescriptors, [owner](const panel_descriptor &entry) {
            return entry.owner == owner;
        });
        return before - mDescriptors.size();
    }

    const panel_descriptor *panel_registry::find(std::string_view name) const noexcept {
        for (const panel_descriptor &entry : mDescriptors) {
            if (entry.name == name) {
                return &entry;
            }
        }
        return nullptr;
    }
}
