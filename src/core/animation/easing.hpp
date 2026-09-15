#pragma once

#include <cmath>
#include <string>
#include <functional>
#include <algorithm>

namespace miquland {

class Easing {
public:
    static float linear(float t) {
        return std::clamp(t, 0.0f, 1.0f);
    }

    static float ease_out_cubic(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        float f = 1.0f - t;
        return 1.0f - f * f * f;
    }

    static float ease_in_out_quad(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
    }

    static float ease_out_quad(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return 1.0f - (1.0f - t) * (1.0f - t);
    }

    static float ease_out_expo(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return (t >= 1.0f) ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
    }

    static float ease_out_back(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        constexpr float c1 = 1.70158f;
        constexpr float c3 = c1 + 1.0f;
        float f = t - 1.0f;
        return 1.0f + c3 * f * f * f + c1 * f * f;
    }

    // Cubic Bezier (x1, y1, x2, y2) solver
    static float cubic_bezier(float x1, float y1, float x2, float y2, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;

        // Binary search parameter u for target x = t
        float low = 0.0f, high = 1.0f;
        float u = t;
        for (int i = 0; i < 16; ++i) {
            float u2 = u * u;
            float u3 = u2 * u;
            float one_minus_u = 1.0f - u;
            float one_minus_u2 = one_minus_u * one_minus_u;

            // B_x(u) = 3*(1-u)^2*u*x1 + 3*(1-u)*u^2*x2 + u^3
            float cur_x = 3.0f * one_minus_u2 * u * x1 + 3.0f * one_minus_u * u2 * x2 + u3;
            if (std::abs(cur_x - t) < 0.001f) break;
            if (cur_x < t) {
                low = u;
            } else {
                high = u;
            }
            u = 0.5f * (low + high);
        }

        // B_y(u)
        float u2 = u * u;
        float u3 = u2 * u;
        float one_minus_u = 1.0f - u;
        float one_minus_u2 = one_minus_u * one_minus_u;
        return 3.0f * one_minus_u2 * u * y1 + 3.0f * one_minus_u * u2 * y2 + u3;
    }

    static std::function<float(float)> from_name(const std::string& name) {
        if (name == "linear") return linear;
        if (name == "ease_out_quad") return ease_out_quad;
        if (name == "ease_in_out_quad") return ease_in_out_quad;
        if (name == "ease_out_expo") return ease_out_expo;
        if (name == "ease_out_back") return ease_out_back;
        if (name == "overshoot" || name == "spring") return ease_out_back;
        // Default: ease_out_cubic
        return ease_out_cubic;
    }
};

} // namespace miquland
