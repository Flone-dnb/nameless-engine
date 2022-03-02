#pragma once

// STL.
#include <memory>
#include <vector>
#include <unordered_map>

namespace dxe {
    class Window;
    class UniqueValueGenerator;

    /**
     * Describes the application instance.
     */
    class Application {
    public:
        Application(const Application &) = delete;
        Application &operator=(const Application &) = delete;
        virtual ~Application();

        /**
         * Returns a reference to the application instance.
         * If no instance was created yet, this function will create it
         * and return a reference to it.
         *
         * @return Reference to the application instance.
         */
        static Application &get();

        /**
         * Creates a main window and starts the message loop for it and all other
         * created windows.
         * After there are no windows (if all windows were closed, for example)
         * this function will return.
         */
        void run();

        /**
         * Tries to find created window with the specified name.
         *
         * @param sWindowName   Name of the window to find.
         *
         * @return Nullptr if the window was not found, valid pointer otherwise.
         */
        [[nodiscard]] Window *getWindowByName(const std::string_view sWindowName) const;

    private:
        Application() = default;

        /**
         * Creates the main window to draw graphics to.
         */
        void createMainWindow();

        /** All windows that we created. */
        std::vector<std::unique_ptr<Window>> vCreatedWindows;
    };
} // namespace dxe