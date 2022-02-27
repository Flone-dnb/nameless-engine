#include "Application.h"

// STL.
#include <memory>
#include <stdexcept>

// Custom.
#include "Window.h"
#include "Error.h"

namespace dxe {
    Application::~Application() {}

    Application &Application::get() {
        static Application application;
        return application;
    }

    void Application::run() {
        createMainWindow();

        while (vCreatedWindows.size() != 0) {
            for (std::vector<std::unique_ptr<Window>>::iterator it = vCreatedWindows.begin();
                 vCreatedWindows.size() > 0 && it != vCreatedWindows.end();) {
                if ((*it)->processNextWindowMessage()) {
                    vCreatedWindows.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    Window *Application::getWindowByName(const std::string_view sWindowName) const {
        for (const auto &pWindow : vCreatedWindows) {
            if (pWindow->getName() == sWindowName) {
                return pWindow.get();
            }
        }

        return nullptr;
    }

    void Application::createMainWindow() {
        // Create main window.
        std::variant<std::unique_ptr<Window>, Error> result =
            Window::newInstance(800, 600, "Main Window", false, true);
        if (std::holds_alternative<Error>(result)) {
            Error error = std::get<Error>(std::move(result));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getError());
        }

        vCreatedWindows.push_back(std::get<std::unique_ptr<Window>>(std::move(result)));
    }
} // namespace dxe