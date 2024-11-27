#pragma once

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/hash.hpp"

#include "core/serialization/archive.h"

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

using IntVec2 = glm::ivec2;
using IntVec3 = glm::ivec3;
using IntVec4 = glm::ivec4;

using UintVec2 = glm::uvec2;

using Mat4 = glm::mat4;

inline Mat4 Translate(const Mat4 &m, const Vec3 &v) { return glm::translate(m, v); }

inline Mat4 Scale(const Mat4 &m, const Vec3 &v) { return glm::scale(m, v); }

inline Mat4 Rotate(const Mat4 &m, float angle, const Vec3 &v) { return glm::rotate(m, angle, v); }

inline Mat4 Lookat(const Vec3 &eye, const Vec3 &centre, const Vec3 &up) { return glm::lookAt(eye, centre, up); }

inline Mat4 Perspective(float fovy, float aspect, float near, float far) { return glm::perspective(fovy, aspect, near, far) * glm::mat4(); }

template <int L, typename T, glm::qualifier Q>
glm::vec<L, T, Q> Normalize(glm::vec<L, T, Q> const &x) { return glm::normalize(x); }

inline float Radians(float degrees) { return glm::radians(degrees); }


inline Archive &operator<<(Archive &Ar, Vec3 &V)
{
    Ar << V.x;
    Ar << V.y;
    Ar << V.z;
    return Ar;
}

inline Archive &operator<<(Archive &Ar, IntVec3 &V)
{
    Ar << V.x;
    Ar << V.y;
    Ar << V.z;
    return Ar;
}