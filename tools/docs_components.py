"""Functional editor documentation components, independent of engine build modules."""

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Component:
    slug: str
    title: str
    description: str
    libraries: tuple[str, ...]

    @property
    def page(self):
        return Path("handbook/editor") / (self.slug + ".md")


EDITOR_COMPONENTS = (
    Component("applications", "Applications",
              "Editor startup, application state, project trust, play mode, and headless execution.",
              ("conjure_editor", "conjure::editor_core")),
    Component("commands-and-undo", "Commands and Undo",
              "Command registration, validation, dispatch, entity and property references, and undo history.",
              ("conjure::editor_core",)),
    Component("protocol", "Protocol",
              "Editor requests, responses, events, transports, access tokens, and audit records.",
              ("conjure::editor_core",)),
    Component("user-interface", "User Interface",
              "The docking workspace, panels, selection, camera, picking, screenshots, and property editing.",
              ("conjure::editor_core", "conjure::editor_ui")),
)
BY_SLUG = {item.slug: item for item in EDITOR_COMPONENTS}
APPLICATION_HEADERS = {
    "editor_application.h", "editor_context.h", "editor_error.h", "editor_module_consumer.h",
    "editor_options.h", "headless_runner.h", "log_category.h", "play_controller.h", "project_trust.h",
}
COMMAND_HEADERS = {
    "builtin_commands.h", "command_desc.h", "command_dispatcher.h", "command_registry.h", "editor_command.h",
    "entity_ref.h", "reflection_path.h", "schema_validator.h", "undo_stack.h",
}
UI_HEADERS = {
    "console_buffer.h", "editor_camera.h", "panel_desc.h", "picker.h", "screenshot_service.h", "selection_set.h",
}


def editor_component(relative):
    """Classify a path relative to conjure/editor; require deliberate placement of new root headers."""
    path = Path(relative)
    if path.parts[0] == "protocol" or path.as_posix() == "event_broadcaster.h":
        return BY_SLUG["protocol"]
    if path.parts[0] == "ui" or path.as_posix() in UI_HEADERS:
        return BY_SLUG["user-interface"]
    if path.as_posix() in COMMAND_HEADERS:
        return BY_SLUG["commands-and-undo"]
    if path.as_posix() in APPLICATION_HEADERS:
        return BY_SLUG["applications"]
    raise ValueError("Assign the editor header to a documentation component: " + path.as_posix())


def component_pages(root):
    headers = (Path(root) / "engine/include/conjure/editor").rglob("*.h")
    return {editor_component(path.relative_to(Path(root) / "engine/include/conjure/editor")).page for path in headers}
