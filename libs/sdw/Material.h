#pragma once

#include "Colour.h"
#include "TextureMap.h"

class Material {
public:
    Colour colour;
    bool mirror;
    std::string texturePath;
    Material();
    std::string name;
    Material(Colour colour,  std::string texturePath, std::string name);
};