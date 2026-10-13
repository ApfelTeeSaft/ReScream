#pragma once

#include <cstdint>

namespace Diesel {

struct Vector2 {
    float x;
    float y;
};

struct Vector3 {
    float x;
    float y;
    float z;
};

struct Quaternion {
    float x;
    float y;
    float z;
    float w;
};

struct Matrix4 {
    float m[4][4];
};

static_assert(sizeof(Vector2) == 8, "Diesel::Vector2 layout");
static_assert(sizeof(Vector3) == 12, "Diesel::Vector3 layout");
static_assert(sizeof(Quaternion) == 16, "Diesel::Quaternion layout");
static_assert(sizeof(Matrix4) == 64, "Diesel::Matrix4 layout");

} // namespace Diesel
