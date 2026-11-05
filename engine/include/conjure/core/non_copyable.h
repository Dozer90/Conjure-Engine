#pragma once

namespace conjure::core {

    /// <summary>
    /// Base class that removes copy operations while keeping moves.
    /// </summary>
    class non_copyable {
        protected:
            non_copyable() = default;
            ~non_copyable() = default;

        public:
            non_copyable(const non_copyable &) = delete;
            non_copyable &operator=(const non_copyable &) = delete;

            non_copyable(non_copyable &&) noexcept = default;
            non_copyable &operator=(non_copyable &&) noexcept = default;
    };

    /// <summary>
    /// Base class that removes both copy and move operations. Use for singletons and objects
    /// whose address is handed out.
    /// </summary>
    class non_movable {
        protected:
            non_movable() = default;
            ~non_movable() = default;

        public:
            non_movable(const non_movable &) = delete;
            non_movable &operator=(const non_movable &) = delete;

            non_movable(non_movable &&) = delete;
            non_movable &operator=(non_movable &&) = delete;
    };
}
