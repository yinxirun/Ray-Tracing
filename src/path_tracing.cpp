#include "path_tracing.h"
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

#include "glm/glm.hpp"
#include "global.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

PathTracingSolver::PathTracingSolver(unsigned spp, float rr)
    : u(0, 1), sampleNum(spp), rr(rr) {}

void PathTracingSolver::SetCamera(Camera *c) { camera = c; }

Ray PathTracingSolver::emitRay(unsigned x, unsigned y) {
  float w = static_cast<float>(camera->width);
  float h = static_cast<float>(camera->height);
  float aspect = w / h;

  float alpha = 0;
  float beta = 0;

  float p = 1 / tanf(camera->fovy / 2);
  float q = (h - 2 * y - 1 + 2 * alpha) / h;
  float r = (2 * x + 1 - w + 2 * beta) * aspect / w;
  glm::vec3 right = glm::normalize(glm::cross(camera->forward, camera->up));
  glm::vec3 direction =
      glm::normalize(p * camera->forward + q * camera->up + r * right);
  return Ray(camera->position, direction);
}

void PathTracingSolver::RenderAndSave(Scene &scene, BVHAccel &bvh,
                                      bool denoise) {
  // 准备
  unsigned width = camera->width;
  unsigned height = camera->height;
  triangleIDImage.resize(width * height);
  positionImage.resize(width * height);
  normalImage.resize(width * height);
  colorImage.resize(width * height);
  ldrImage.resize(width * height);
  if (denoise) {
    colorImageDenoised.resize(width * height);
  }

  // 生成光线
  std::vector<Ray> rays;
  for (unsigned i = 0; i < height; ++i) {
    for (unsigned j = 0; j < width; ++j) {
      rays.push_back(emitRay(j, i));
    }
  }

  // 生成 Geometry Buffer
  std::vector<const Material *> materials(width * height, nullptr);
  for (unsigned i = 0; i < width * height; ++i) {
    SurfaceInteraction info;
    bool hit = bvh.Intersect(rays[i], &info);
    if (!hit) {
      triangleIDImage[i] = UINT32_MAX;
      normalImage[i] = glm::vec3(0, 0, 0);
      positionImage[i] = glm::vec3(0, 0, 0);
    } else {
      glm::vec3 hitPoint = rays[i].origin + info.time * rays[i].direction;
      glm::vec4 viewSpacePoint = camera->View() * glm::vec4(hitPoint, 1.0f);
      glm::vec4 clipSpaceHitPoint = camera->Perspective() * viewSpacePoint;

      triangleIDImage[i] = info.hitID;
      normalImage[i] = info.normal;
      positionImage[i] = hitPoint;
      materials[i] = info.material;
    }
  }
  SaveNormalImage("Normal.bmp");
  SavePositionImage("Position.bmp");
  SaveTriangleIdImage("TriangleId.bmp");

  // 追踪并保存中间结果
  std::vector<glm::vec3> tempFrame(colorImage);
  lights = bvh.getAllLights();

  auto threadNum = std::thread::hardware_concurrency();

  for (size_t s = 0; s < sampleNum; ++s) {
    std::vector<std::thread> threads;

    auto kernel = [&](size_t tid, unsigned totalPixels) {
      for (size_t index = tid; index < totalPixels; index += threadNum) {
        if (triangleIDImage[index] == UINT32_MAX) {
          tempFrame[index] += glm::vec3(0, 0, 0);
        } else {
          tempFrame[index] +=
              shade(bvh, positionImage[index], normalImage[index],
                    materials[index], -rays[index].direction);
        }
      }
    };

    for (unsigned i = 0; i < threadNum; ++i) {
      threads.push_back(std::thread(kernel, i, width * height));
    }
    for (unsigned i = 0; i < threadNum; ++i) {
      threads[i].join();
    }

    if ((s & (s + 1)) == 0) {
      for (int i = 0; i < colorImage.size(); ++i) {
        colorImage[i] = tempFrame[i] / (float)(s + 1);
      }
      std::string fileName = "./output/" + std::to_string(s) + ".exr";
      ToneMapping(colorImage);
      SaveColorImageHDR(fileName, colorImage);
    }
  }

  for (int i = 0; i < colorImage.size(); ++i) {
    colorImage[i] = tempFrame[i] / (float)(sampleNum);
  }

  if (denoise) {
    Denoise();
    SaveColorImageHDR("./output/FinalDenoised.exr", colorImageDenoised);
    ToneMapping(colorImageDenoised);
    SaveColorImageLDR("Final.bmp");
  } else {
    ToneMapping(colorImage);
    SaveColorImageLDR("Final.bmp");
  }
}

glm::vec3 PathTracingSolver::shade(BVHAccel &bvh, glm::vec3 hitPoint,
                                   glm::vec3 n, const Material *m,
                                   glm::vec3 out) {
  if (m->type == MaterialType::EMISSIVE) {
    return m->emission;
  }

  glm::vec3 illuminationDirect{0, 0, 0};

  for (auto &p : lights) {
    Triangle *l = dynamic_cast<Triangle *>(p);
    glm::vec3 lightPoint = l->RandomPoint();
    // lightPoint = l->Center();
    glm::vec3 lightNormal = l->GetNormal();
    const Material *lightMaterial = l->GetMaterial();
    glm::vec3 newDirection = glm::normalize(lightPoint - hitPoint);

    SurfaceInteraction info;
    Ray ray(hitPoint, newDirection);
    bool hit = bvh.Intersect(ray, &info);
    if (!hit) continue;
    if (info.hitID != l->GetID()) continue;
    float dist = info.time;
    float area = l->Area();
    float cosine0 = glm::dot(n, newDirection);
    float cosine1 = -glm::dot(lightNormal, newDirection);
    if (cosine0 < 0 || cosine1 < 0) continue;
    illuminationDirect += lightMaterial->emission *
                          BRDF(-newDirection, out, m) * cosine0 * cosine1 *
                          area / dist / dist;
  }

  float death = u(e);
  if (death > rr) return illuminationDirect;

  glm::vec3 illuminationIndirect{0, 0, 0};

  glm::vec3 direction;
  float pdf;
  Sample(m, n, direction, pdf);

  Ray ray(hitPoint, direction);
  SurfaceInteraction info;
  bool hit = bvh.Intersect(ray, &info);

  if (!hit)
    illuminationIndirect = glm::vec3(0, 0, 0);
  else {
    if (info.material->emission.x > 0 || info.material->emission.y > 0 ||
        info.material->emission.z > 0) {
      illuminationIndirect = glm::vec3(0);
    } else {
      glm::vec3 newHitPoint = ray.origin + info.time * ray.direction;
      illuminationIndirect =
          BRDF(-ray.direction, out, m) *
          shade(bvh, newHitPoint, info.normal, info.material, -ray.direction) *
          glm::dot(n, ray.direction) / pdf / rr;
    }
  }
  return illuminationDirect + illuminationIndirect;
}

void PathTracingSolver::SaveColorImageLDR(std::string fileName) {
  unsigned width = camera->width;
  unsigned height = camera->height;

  std::vector<unsigned char> img(width * height * 3);
  for (int i = 0; i < ldrImage.size(); i++) {
    img[i * 3 + 0] = ldrImage[i].x;
    img[i * 3 + 1] = ldrImage[i].y;
    img[i * 3 + 2] = ldrImage[i].z;
  }

  stbi_write_bmp(fileName.c_str(), width, height, 3, img.data());
}

glm::vec3 PathTracingSolver::BRDF(glm::vec3 in, glm::vec3 out,
                                  const Material *material) {
  return material->albedo / pi;
}

void PathTracingSolver::Sample(const Material *material, glm::vec3 normal,
                               glm::vec3 &direction, float &pdf) {
  float theta, phi;

  glm::vec3 t = glm::cross(normal, glm::vec3(0, 1, 0));
  if (glm::dot(t, t) < epsilon) {
    t = glm::cross(normal, glm::vec3(0, 0, 1));
  }
  t = glm::normalize(t);
  glm::vec3 b = glm::normalize(glm::cross(normal, t));

  switch (material->type) {
    case MaterialType::DIFFUSE:
      // 半球余弦权重采样
      theta = acos(1 - 2 * u(e)) / 2;
      phi = 2 * pi * u(e);
      pdf = cos(theta) / pi;
      break;
    default:
      // 半球均匀采样
      theta = acos(1 - u(e));
      phi = 2 * pi * u(e);
      pdf = 1 / 2 / pi;
      break;
  }

  direction = sinf(theta) * cosf(phi) * t + sinf(theta) * sinf(phi) * b +
              cosf(theta) * normal;
}

void PathTracingSolver::Denoise() {
  unsigned threadNum = std::thread::hardware_concurrency();
  std::vector<std::thread> threads;

  unsigned pixelNum = camera->width * camera->height;
  int width = camera->width;
  int height = camera->height;
  constexpr int kernelRadius = 3;
  constexpr float sigmaDistance = 32;
  constexpr float sigmaColor = 0.6;
  constexpr float sigmaNormal = 0.1;
  constexpr float sigmaPlane = 0.1;
  constexpr float k = 0.5;

  std::vector<glm::vec3> tempImage(width * height);
  auto removeOutlier = [&](unsigned tid) {
    for (unsigned centreIndex = tid; centreIndex < pixelNum;
         centreIndex += threadNum) {
      int y = centreIndex / width;
      int x = centreIndex % width;
      glm::vec3 standardDeviation;
      glm::vec3 mean;

      // 计算均值
      glm::vec3 sum = glm::vec3(0);
      float count = 0;
      for (int i = -kernelRadius; i <= kernelRadius; ++i) {
        if (x + i < 0 || x + i >= width) continue;
        for (int j = -kernelRadius; j <= kernelRadius; ++j) {
          if (y + j < 0 || y + j >= height) continue;
          int index = (y + j) * width + i + x;

          if (triangleIDImage[index] == UINT32_MAX) {
            continue;
          }

          sum += colorImage[index];
          count += 1;
        }
      }
      mean = sum / count;

      // 计算方差
      sum = glm::vec3(0);
      for (int i = -kernelRadius; i <= kernelRadius; ++i) {
        if (x + i < 0 || x + i >= width) continue;
        for (int j = -kernelRadius; j <= kernelRadius; ++j) {
          if (y + j < 0 || y + j >= height) continue;
          int index = (y + j) * width + i + x;
          if (triangleIDImage[index] == UINT32_MAX) {
            continue;
          }
          sum += glm::dot(colorImage[index] - mean, colorImage[index] - mean);
        }
      }
      standardDeviation = glm::sqrt(sum / count);

      // 截断
      glm::vec3 color = colorImage[centreIndex];
      for (int i = 0; i < 3; ++i) {
        if (color[i] > mean[i] + k * standardDeviation[i])
          color[i] = mean[i] + k * standardDeviation[i];
        if (color[i] < mean[i] - k * standardDeviation[i])
          color[i] = mean[i] - k * standardDeviation[i];
      }

      tempImage[centreIndex] = color;
    }
  };

  auto filter = [&](unsigned tid) {
    for (unsigned centreIndex = tid; centreIndex < pixelNum;
         centreIndex += threadNum) {
      int y = centreIndex / width;
      int x = centreIndex % width;
      float weightSum = 0;
      glm::vec3 color = glm::vec3(0);
      for (int i = -kernelRadius; i <= kernelRadius; ++i) {
        if (x + i < 0 || x + i >= width) continue;
        for (int j = -kernelRadius; j <= kernelRadius; ++j) {
          if (y + j < 0 || y + j >= height) continue;
          int index = (y + j) * width + i + x;
          if (triangleIDImage[index] == UINT32_MAX) {
            continue;
          }
          // distance
          float distanceWeight =
              (-i * i - j * j) / (2 * sigmaDistance * sigmaDistance);

          // color
          glm::vec3 colorDifferece = tempImage[index] - tempImage[index];
          float colorWeight = -glm::dot(colorDifferece, colorDifferece) /
                              (2 * sigmaColor * sigmaColor);

          // normal
          float d =
              glm::acos(glm::dot(normalImage[centreIndex], normalImage[index]));
          float normalWeight = -d * d / (2 * sigmaNormal * sigmaNormal);

          // plane
          glm::vec3 displacement =
              positionImage[index] - positionImage[centreIndex];
          float length = glm::sqrt(glm::dot(displacement, displacement));
          if (length < epsilon)
            d = 0;
          else
            d = glm::dot(normalImage[centreIndex], displacement / length);
          float planeWeight = -d * d / (2 * sigmaPlane * sigmaPlane);

          float weight =
              exp(distanceWeight + colorWeight + planeWeight + normalWeight);

          color += weight * tempImage[index];
          weightSum += weight;
        }
      }
      color /= weightSum;
      colorImageDenoised[centreIndex] = color;
    }
  };

  for (int i = 0; i < threadNum; ++i) {
    threads.push_back(std::thread(removeOutlier, i));
  }
  for (int i = 0; i < threadNum; ++i) {
    threads[i].join();
  }
  threads.clear();

  for (int i = 0; i < threadNum; ++i) {
    threads.push_back(std::thread(filter, i));
  }
  for (int i = 0; i < threadNum; ++i) {
    threads[i].join();
  }
}

void PathTracingSolver::ToneMapping(std::vector<glm::vec3> &hdrImage) {
  float avg = 0;
  for (int i = 0; i < hdrImage.size(); ++i) {
    avg += sqrt(hdrImage[i].x * hdrImage[i].x + hdrImage[i].y * hdrImage[i].y +
                hdrImage[i].z * hdrImage[i].z);
  }
  avg = avg / hdrImage.size();

  float adapted_lum = avg;
  const float A = 2.51f;
  const float B = 0.03f;
  const float C = 2.43f;
  const float D = 0.59f;
  const float E = 0.14f;

  auto clamp = [](float value, float lower, float upper) -> float {
    if (value > upper)
      return upper;
    else if (value < lower)
      return lower;
    else
      return value;
  };

  for (int i = 0; i < hdrImage.size(); i++) {
    for (int j = 0; j < 3; ++j) {
      float color = hdrImage[i][j];
      color *= 1.3;
      ldrImage[i][j] =
          (color * (A * color + B)) / (color * (C * color + D) + E) * 255.0f;
      ldrImage[i][j] = clamp(ldrImage[i][j], 0, 255);
    }
  }
}

void PathTracingSolver::SaveNormalImage(std::string fileName) {
  unsigned width = camera->width;
  unsigned height = camera->height;
  std::vector<unsigned char> img(width * height * 3);

  for (int i = 0; i < normalImage.size(); i++) {
    glm::vec3 n = normalImage[i] * 0.5f + 0.5f;
    img[i * 3 + 0] = n.x * 255;
    img[i * 3 + 1] = n.y * 255;
    img[i * 3 + 2] = n.z * 255;
  }

  stbi_write_bmp(fileName.c_str(), width, height, 3, img.data());
}

void PathTracingSolver::SavePositionImage(std::string fileName) {
  unsigned width = camera->width;
  unsigned height = camera->height;
  glm::mat4 vp = camera->Perspective() * camera->View();
  std::vector<unsigned char> img(width * height * 3);

  for (int i = 0; i < positionImage.size(); i++) {
    glm::vec4 p = vp * glm::vec4(positionImage[i], 1.f);
    p = p / p.w * 0.5f + 0.5f;
    img[i * 3 + 0] = p.x * 255;
    img[i * 3 + 1] = p.y * 255;
    img[i * 3 + 2] = p.z * 255;
  }
  stbi_write_bmp(fileName.c_str(), width, height, 3, img.data());
}

void PathTracingSolver::SaveTriangleIdImage(std::string fileName) {
  unsigned width = camera->width;
  unsigned height = camera->height;
  std::vector<unsigned char> img(width * height * 3);

  glm::uint32 num = 0;
  for (int i = 0; i < triangleIDImage.size(); i++) {
    glm::uint32 id = triangleIDImage[i];
    if (id != UINT32_MAX && id + 1 > num) num = id + 1;
  }

  std::vector<glm::ivec3> colors(num);
  for (auto &c : colors) {
    c.x = rand() % 255;
    c.y = rand() % 255;
    c.z = rand() % 255;
  }

  for (int i = 0; i < triangleIDImage.size(); i++) {
    glm::uint32 id = triangleIDImage[i];
    if (id == UINT32_MAX) {
      img[i * 3 + 0] = 0;
      img[i * 3 + 1] = 0;
      img[i * 3 + 2] = 0;
    } else {
      img[i * 3 + 0] = colors[id].x;
      img[i * 3 + 1] = colors[id].y;
      img[i * 3 + 2] = colors[id].z;
    }
  }
  stbi_write_bmp(fileName.c_str(), width, height, 3, img.data());
}

void PathTracingSolver::SaveColorImageHDR(std::string fileName,
                                          std::vector<glm::vec3> &hdrImage) {
  unsigned width = camera->width;
  unsigned height = camera->height;

  EXRHeader header;
  InitEXRHeader(&header);

  EXRImage image;
  InitEXRImage(&image);

  image.num_channels = 3;

  std::vector<float> images[3];
  images[0].resize(width * height);
  images[1].resize(width * height);
  images[2].resize(width * height);

  for (int i = 0; i < width * height; i++) {
    images[0][i] = hdrImage[i].x;
    images[1][i] = hdrImage[i].y;
    images[2][i] = hdrImage[i].z;
  }

  float *image_ptr[3];
  image_ptr[0] = &(images[2].at(0));  // B
  image_ptr[1] = &(images[1].at(0));  // G
  image_ptr[2] = &(images[0].at(0));  // R

  image.images = (unsigned char **)image_ptr;
  image.width = width;
  image.height = height;

  header.num_channels = 3;
  header.channels =
      (EXRChannelInfo *)malloc(sizeof(EXRChannelInfo) * header.num_channels);
  // Must be BGR(A) order, since most of EXR viewers expect this channel order.
  strncpy(header.channels[0].name, "B", 255);
  header.channels[0].name[strlen("B")] = '\0';
  strncpy(header.channels[1].name, "G", 255);
  header.channels[1].name[strlen("G")] = '\0';
  strncpy(header.channels[2].name, "R", 255);
  header.channels[2].name[strlen("R")] = '\0';

  header.pixel_types = (int *)malloc(sizeof(int) * header.num_channels);
  header.requested_pixel_types =
      (int *)malloc(sizeof(int) * header.num_channels);
  for (int i = 0; i < header.num_channels; i++) {
    header.pixel_types[i] =
        TINYEXR_PIXELTYPE_FLOAT;  // pixel type of input image
    header.requested_pixel_types[i] =
        TINYEXR_PIXELTYPE_HALF;  // pixel type of output image to be stored in
                                 // .EXR
  }

  const char *err;
  int ret = SaveEXRImageToFile(&image, &header, fileName.c_str(), &err);
  if (ret != TINYEXR_SUCCESS) {
    fprintf(stderr, "Save EXR err: %s\n", err);
  }
  printf("Saved exr file. [ %s ] \n", fileName.c_str());

  free(header.channels);
  free(header.pixel_types);
  free(header.requested_pixel_types);
}
