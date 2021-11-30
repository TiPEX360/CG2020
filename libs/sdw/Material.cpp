#include "Material.h"

Material::Material() {
    colour = Colour();
    mirror = false;
}

Material::Material(Colour colour,  std::string texturePath, std::string name) {
    Material::colour = colour;
    Material::texturePath = texturePath;
    mirror = false;
    Material::name = name;
}