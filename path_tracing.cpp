#include "path_tracing.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

#include "glm/glm.hpp"

extern const float epsilon = 0.001;

const float pi = 3.1415926535;

extern std::fstream recorder;

Ray::Ray(glm::vec3 o, glm::vec3 d) : origin(o), direction(d) {}

IntersectionInfo::IntersectionInfo() {
  face = nullptr;
  time = FLT_MAX;
  intersection = false;
}

Camera::Camera() {
  // veach-mis
  // glm::vec3 eye = glm::vec3(28.2792, 5.2, 1.23612e-06);
  // glm::vec3 lookat = glm::vec3(0, 2.8, 0);
  // glm::vec3 up = glm::vec3(0, 1, 0);
  // fovy = 20.1143 / 180.f * pi;

  // cornell box
  glm::vec3 eye = glm::vec3(278, 273, -800);
  glm::vec3 lookat = glm::vec3(278, 273, -799);
  glm::vec3 up = glm::vec3(0, 1, 0);
  fovy = 39.3077 / 180.f * pi;

  // stairscase
  // glm::vec3 eye =
  // glm::vec3(6.9118194580078125, 1.6516278982162476, 2.5541365146636963);
  // glm::vec3 lookat = glm::vec3(2.328019380569458, 1.6516276597976685,
  // 0.33640459179878235); glm::vec3 up = glm::vec3(0, 1, 0); fovy = 42.9957 /
  // 180.f * pi;

  this->position = eye;
  this->forward = glm::normalize(lookat - eye);
  glm::vec3 right = glm::cross(this->forward, up);
  this->up = glm::normalize(glm::cross(right, forward));
}

IntersectionInfo Ray::intersect(const Triangle* face) {
  IntersectionInfo info;
  info.intersection = false;
  glm::vec3 a = face->getPosition(1) - face->getPosition(0);
  glm::vec3 b = face->getPosition(2) - face->getPosition(0);
  glm::vec3 c = origin - face->getPosition(0);
  float D = direction.x * a.y * b.z + b.x * direction.y * a.z +
            a.x * b.y * direction.z - a.x * direction.y * b.z -
            b.x * a.y * direction.z - direction.x * b.y * a.z;

  if (D < epsilon && D > -epsilon) return info;
  float alpha = (direction.x * c.y * b.z + b.x * direction.y * c.z +
                 c.x * b.y * direction.z - c.x * direction.y * b.z -
                 b.x * c.y * direction.z - direction.x * b.y * c.z) /
                D;
  float beta = (direction.x * a.y * c.z + c.x * direction.y * a.z +
                a.x * c.y * direction.z - a.x * direction.y * c.z -
                c.x * a.y * direction.z - direction.x * c.y * a.z) /
               D;
  float t = -(c.x * a.y * b.z + b.x * c.y * a.z + a.x * b.y * c.z -
              a.x * c.y * b.z - b.x * a.y * c.z - c.x * b.y * a.z) /
            D;
  if (t < epsilon || alpha < 0 || beta < 0 || alpha + beta > epsilon + 1)
    return info;
  info.intersection = true;
  info.time = t;
  info.face = face;
  return info;
}

void Ray::intersect(BVH& bvh, const bvhNode* node, IntersectionInfo& info) {
  std::array<float, 3> tmin, tmax;
  for (unsigned i = 0; i < 3; ++i) {
    if (direction[i] > epsilon || direction[i] < epsilon) {
      tmin[i] = (node->box[0][i] - origin[i]) / direction[i];
      tmax[i] = (node->box[1][i] - origin[i]) / direction[i];
      if (tmin[i] > tmax[i]) std::swap(tmin[i], tmax[i]);
    } else {
      tmin[i] = FLT_MIN;
      tmax[i] = FLT_MAX;
    }
  }

  float in = std::max(std::max<float>(tmin[0], tmin[1]), tmin[2]);
  float out = std::min(std::min<float>(tmax[0], tmax[1]), tmax[2]);
  if (in > out + epsilon) return;
  if (out < epsilon) return;  // box is behind the light

  if (node->left == nullptr && node->right == nullptr) {
    for (auto e : node->faceIndices) {
      const Triangle* face = bvh.getFace(e);
      IntersectionInfo result = intersect(face);
      if (result.intersection == true && result.time < info.time) {
        info = result;
      }
    }
  } else if (node->left != nullptr && node->right != nullptr) {
    intersect(bvh, node->left, info);
    intersect(bvh, node->right, info);
  } else {
    throw std::exception();
  }
}

PathTracingSolver::PathTracingSolver(unsigned w, unsigned h) : width(w), height(h) {
  frame.resize(width * height);
  sampleNum = 256;
  std::uniform_real_distribution<float> d0(0, 1);
  std::uniform_real_distribution<float> d1(0, 2 * pi);
  u0.param(d0.param());
  u1.param(d1.param());
}

Ray PathTracingSolver::emitRay(unsigned x, unsigned y, Camera& camera) {
  float w = width;
  float h = height;
  float aspect = w / h;
  float p = 1 / tanf(camera.fovy / 2);
  float q = (2 * y + 1 - h) / h;
  float r = (2 * x + 1 - w) * aspect / w;
  glm::vec3 right = glm::normalize(glm::cross(camera.forward, camera.up));
  glm::vec3 direction =
      glm::normalize(p * camera.forward + q * camera.up + r * right);
  return Ray(camera.position, direction);
}

void PathTracingSolver::draw(Scene& scene, BVH& bvh, Camera& camera) {
  std::vector<Ray> rays;
  for (unsigned i = 0; i < width; ++i) {
    for (unsigned j = 0; j < height; ++j) {
      rays.push_back(emitRay(i, j, camera));
    }
  }

  std::vector<glm::vec3> tempFrame(frame);
  for (size_t s = 0; s < sampleNum; ++s) {
    for (size_t i = 0; i < width; ++i) {
      for (size_t j = 0; j < height; ++j) {
        // if (i == 7 && j == 3) {
        //   s -= 1;
        // }
        unsigned index = i * height + j;
        IntersectionInfo info;
        rays[index].intersect(bvh, bvh.getRootNode(), info);
        if (!info.intersection)
          tempFrame[index] = glm::vec3(0, 0, 0);
        else {
          const Material* m = info.face->getMaterial();
          if (m->emissive.x > epsilon || m->emissive.y > epsilon ||
              m->emissive.z > epsilon) {
            tempFrame[i * height + j] += m->emissive;
          } else {
            glm::vec3 hitPoint =
                rays[index].origin + info.time * rays[index].direction;
            tempFrame[index] +=
                shade(scene, bvh, hitPoint, info.face, -rays[index].direction);
          }
        }
        // recorder << i << " " << j << " " << tempFrame[index][0] << " "
        //          << tempFrame[index][1] << " " << tempFrame[index][2]
        //          << std::endl;
      }
    }
    for (int i = 0; i < frame.size(); ++i) {
      frame[i] = tempFrame[i] / (float)(s + 1);
    }
    std::string fileName = "./output/" + std::to_string(s) + ".bmp";
    save(fileName);
    std::cout << s << std::endl;
  }
}

void PathTracingSolver::save(std::string fileName) {
  std::string bak = fileName + ".bak";
  std::fstream f(bak, std::ios::out | std::ios::binary);
  f.write((char*)frame.data(), sizeof(glm::vec3) * width * height);
  f.close();

  // b g r
  toneMapping();
  int w = this->width;
  int h = this->height;
  int l = (w * 3 + 3) / 4 * 4;
  int bmi[] = {l * w + 54, 0,     54, 40, w,   h, 1 | 3 * 8 << 16,
               0,          l * h, 0,  0,  100, 0};

  std::vector<unsigned char> img(width * height * 3);
  for (int i = 0; i < width; i++) {
    for (int j = 0; j < height; ++j) {
      img[(j * width + i) * 3 + 0] = frame[i * height + j].z;
      img[(j * width + i) * 3 + 1] = frame[i * height + j].y;
      img[(j * width + i) * 3 + 2] = frame[i * height + j].x;
    }
  }
  FILE* fp = fopen(fileName.c_str(), "wb");
  fprintf(fp, "BM");
  fwrite(&bmi, 52, 1, fp);
  fwrite(img.data(), 1, l * height, fp);
  fclose(fp);
}

glm::vec3 PathTracingSolver::BRDF(glm::vec3 in, glm::vec3 out,
                                const Material* material) {
  return material->albedo / pi;
}

float clamp(float value, float lower, float upper) {
  if (value > upper)
    return upper;
  else if (value < lower)
    return lower;
  else
    return value;
}

void PathTracingSolver::toneMapping() {
  float avg = 0;
  for (int i = 0; i < frame.size(); ++i) {
    avg += sqrt(frame[i].x * frame[i].x + frame[i].y * frame[i].y +
                frame[i].z * frame[i].z);
  }
  avg = avg / frame.size();

  float adapted_lum = avg;
  const float A = 2.51f;
  const float B = 0.03f;
  const float C = 2.43f;
  const float D = 0.59f;
  const float E = 0.14f;
  for (int i = 0; i < frame.size(); i++) {
    for (int j = 0; j < 3; ++j) {
      float color = frame[i][j];
      color *= 1.3;
      frame[i][j] =
          (color * (A * color + B)) / (color * (C * color + D) + E) * 255.0f;
      frame[i][j] = clamp(frame[i][j], 0, 254);
    }
  }
}

glm::vec3 PathTracingSolver::shade(Scene& scene, BVH& bvh, glm::vec3 hitPoint,
                                 const Triangle* face, glm::vec3 out) {
  glm::vec3 n = face->getNormal();
  const Material* m = face->getMaterial();

  glm::vec3 illuminationDirect{0, 0, 0};
  auto lights = bvh.getAllLights();
  for (auto& l : lights) {
    IntersectionInfo info;
    glm::vec3 lightPoint = l->getRandomPoint();
    glm::vec3 lightNormal = l->getNormal();
    const Material* lightMaterial = l->getMaterial();
    glm::vec3 newDirection = glm::normalize(lightPoint - hitPoint);
    Ray ray(hitPoint, newDirection);
    ray.intersect(bvh, bvh.getRootNode(), info);
    if (!info.intersection) continue;
    if (info.face->id != l->id) continue;
    float dist = info.time;
    float area = l->getArea();
    float cosine0 = glm::dot(n, newDirection);
    float cosine1 = -glm::dot(lightNormal, newDirection);
    if (cosine0 < 0 || cosine1 < 0) continue;
    illuminationDirect += lightMaterial->emissive *
                          BRDF(-newDirection, out, m) * cosine0 * cosine1 *
                          area / dist / dist;
  }

  float death = u0(e);
  if (death < rr || false) return illuminationDirect;
  glm::vec3 illuminationIndirect{0, 0, 0};
  glm::vec3 t = glm::normalize(face->getPosition(1) - face->getPosition(0));
  glm::vec3 b = glm::normalize(glm::cross(n, t));
  t = glm::normalize(glm::cross(b, n));
  float theta = acos(u0(e));
  float phi = u1(e);
  glm::vec3 newDirection = sinf(theta) * cosf(phi) * t +
                           sinf(theta) * sinf(phi) * b + cosf(theta) * n;
  Ray ray(hitPoint, newDirection);
  IntersectionInfo info;
  ray.intersect(bvh, bvh.getRootNode(), info);
  if (!info.intersection)
    return glm::vec3(0, 0, 0);
  else {
    if (m->emissive.x > epsilon || m->emissive.y > epsilon ||
        m->emissive.z > epsilon)
      return glm::vec3(0, 0, 0);
    else {
      glm::vec3 newHitPoint = ray.origin + info.time * ray.direction;
      illuminationIndirect =
          BRDF(-ray.direction, out, m) *
          shade(scene, bvh, newHitPoint, info.face, -ray.direction) *
          glm::dot(n, ray.direction) * 2.0f * pi / rr;
    }
  }
  return illuminationDirect + illuminationIndirect;
}