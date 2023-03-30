#define STB_IMAGE_IMPLEMENTATION
#include "Material.h"

#include <iostream>

#include "std_image.h"

void Material::bind_map(std::string filepath) {
  int width, height, nrChannels;
  map = stbi_load(filepath.c_str(), &width, &height, &nrChannels, 0);
  if (map == NULL) {
    std::cout << "Fail to load texture mapping. " << std::endl;
    return;
  }
  map_width = width;
  map_height = height;
  map_nrChannels = nrChannels;
  for (int i = 0; i < width * height * 3; i += 3) {
    unsigned char temp = map[i];
    map[i] = map[i + 2];
    map[i + 2] = temp;
  }
}

Material::~Material() { stbi_image_free(map); }

Material::Material()
    : map_width(0), map_height(0), map_nrChannels(0), map(NULL) {}