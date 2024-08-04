#include "vertex.h"

bool Vertex::operator==(const Vertex &other) const {
  return position == other.position && normal == other.normal &&
         texCoord == other.texCoord;
}