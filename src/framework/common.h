#pragma once

#include <vector>
#include <string>

template <typename T>
using Array = std::vector<T>;

using String = std::basic_string<char, std::char_traits<char>>;
using WString = std::basic_string<wchar_t, std::char_traits<wchar_t>>;

template <typename T>
String ToString(const T f, const int n = 6) {
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << f;
    return out.str();
}


#pragma warning(push)
#pragma warning(disable : 4201) // C4201: nonstandard extension used: nameless struct/union
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#pragma warning(pop)

using vec2 = glm::highp_vec2;
using vec3 = glm::highp_vec3;
using vec4 = glm::highp_vec4;
using mat4 = glm::highp_mat4;
using quat = glm::highp_quat;

struct Recti { int left, top, right, bottom; };

static const float MM_Pi = 3.1415926536f;

inline float Rad2Deg(const float rad) {
    return rad * (180.0f / MM_Pi);
}

inline float Deg2Rad(const float deg) {
    return deg * (MM_Pi / 180.0f);
}

template <typename T>
inline T Max(const T& a, const T& b) {
    return a > b ? a : b;
}

template <typename T>
inline T Min(const T& a, const T& b) {
    return a < b ? a : b;
}

template <typename T>
inline T Lerp(const T& a, const T& b, const float t) {
    return a + (b - a) * t;
}

template <typename T>
inline T Clamp(const T& v, const T& minV, const T& maxV) {
    return (v < minV) ? minV : ((v > maxV) ? maxV : v);
}

// these funcs should be compatible with same in GLSL
template <typename T>
inline float Length(const T& v) { return glm::length(v); }
inline float Dot(const vec2& a, const vec2& b) { return glm::dot(a, b); }
inline float Dot(const vec3& a, const vec3& b) { return glm::dot(a, b); }
inline float Dot(const vec4& a, const vec4& b) { return glm::dot(a, b); }
inline vec3 Cross(const vec3& a, const vec3& b) { return glm::cross(a, b); }
inline vec3 Normalize(const vec3& v) { return glm::normalize(v); }

inline quat Normalize(const quat& q) { return glm::normalize(q); }
inline quat QAngleAxis(const float angleRad, const vec3& axis) { return glm::angleAxis(angleRad, axis); }
inline vec3 QRotate(const quat& q, const vec3& v) { return glm::rotate(q, v); }

inline mat4 MatRotate(const float angle, const float x, const float y, const float z) { return glm::rotate(angle, vec3(x, y, z)); }


// aliases to glm's functions
constexpr auto MatOrtho = glm::orthoRH<float>;
constexpr auto MatProjection = glm::perspectiveRH_ZO<float>;
constexpr auto MatLookAt = glm::lookAtRH<float, glm::highp>;

constexpr auto QToMat = glm::toMat4<float, glm::highp>;

