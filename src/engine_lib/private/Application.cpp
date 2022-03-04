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

        while (!vCreatedWindows.empty()) {
            for (auto it = vCreatedWindows.begin();
                 !vCreatedWindows.empty() && it != vCreatedWindows.end();) {
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
        std::variant<std::unique_ptr<Window>, Error> result = Window::getBuilder()
                                                                  .withSize(800, 600)
                                                                  .withName("Main Window")
                                                                  .withVisibility(true)
                                                                  .withFullscreenMode(false)
                                                                  .build();
        if (std::holds_alternative<Error>(result)) {
            Error error = std::get<Error>(std::move(result));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getError());
        }

        vCreatedWindows.push_back(std::get<std::unique_ptr<Window>>(std::move(result)));
    }
} // namespace dxe