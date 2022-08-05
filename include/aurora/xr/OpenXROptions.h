//
// Created by austin on 1/30/22.
//

#pragma once

#include <string>

namespace aurora::xr {
    struct OpenXROptions {
        std::string GraphicsPlugin;

        std::string AppName{"OpenXR Program"};

        std::string FormFactor{"Hmd"};

        std::string ViewConfiguration{"Stereo"};

        std::string EnvironmentBlendMode{"Opaque"};

        std::string AppSpace{"Local"};
    };
}

