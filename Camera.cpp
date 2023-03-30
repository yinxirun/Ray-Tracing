#include "Camera.h"

#include <cmath>
#include <fstream>

extern const float epsilon;

Camera::Camera() : height(0), width(0) {}

Camera::Camera(Vector3D _loc, Vector3D _lookat, Vector3D _up, float _fov,
               enum projection_type _type)
    : location(_loc),
      lookat(_lookat),
      up(_up),
      fov(_fov),
      type(_type),
      height(0),
      width(0) {}

Camera::~Camera() { delete[] image; }

void Camera::set_image_size(unsigned w, unsigned h) {
  if (w == 0 || h == 0) {
    std::cout << "Fail to set pixels. The number of rows and columns must be "
                 "greater than zero. "
              << std::endl;
    return;
  }
  image = new float[w * h * 3];
  if (image == nullptr) {
    std::cout << "Fail to malloc" << std::endl;
    exit(-1);
  }
  width = w;
  height = h;
}

void Camera::set_type(enum projection_type t) { type = t; }
void Camera::set_fov(float _fov) { fov = _fov; }
void Camera::set_lookat(Vector3D& v) { lookat = v; }
void Camera::set_location(Vector3D& v) { location = v; }
void Camera::set_up(Vector3D& v) {
  up = v;
  Vector3D right = Vector3D::cross(lookat, up);
  up = Vector3D::cross(right, lookat);
  up.normalize();
}

void Camera::save_BMP(const char* filename) {
  int w = width;
  int h = height;
  std::fstream file("veach-mis",
                    std::ios::out | std::ios::trunc | std::ios::binary);
  file.write((char*)image, w * h * 3 * sizeof(float));
  file.close();

  unsigned char* rgb = new unsigned char[w * h * 3];
  if (rgb == nullptr) {
    std::cout << "Fail to allocate. " << std::endl;
    return;
  }
  tone_mapping(image, rgb);

  int bmi[] = {
      3 * w * h + 54, 0, 54, 40, w, h, 1 | 3 * 8 << 16, 0, 0, 0, 0, 0, 0};
  FILE* fp = fopen(filename, "wb");
  fprintf(fp, "BM");
  fwrite(&bmi, 52, 1, fp);
  fwrite(rgb, 1, 3 * w * h, fp);
  fclose(fp);
  delete[] rgb;
}

void Camera::tone_mapping(float* in, unsigned char* out) {
  float n = 1;
  float average_r = 0;
  float average_g = 0;
  float average_b = 0;

  for (int i = 0; i < height * width * 3; i += 3) {
    average_r += in[i];
    average_g += in[i + 1];
    average_b += in[i + 2];
  }
  average_b /= (height * width);
  average_r /= (height * width);
  average_g /= (height * width);
  float average = (average_b + average_g + average_r) / 3;
  std::cout << "Average " << average << std::endl;
  std::cout << "Average R " << average_r << std::endl;
  std::cout << "Average G " << average_g << std::endl;
  std::cout << "Average B " << average_b << std::endl;

  float k = 0.8;

  for (int i = 0; i < height * width * 3; i += 3) {
    float r = in[i] / (in[i] + average_r / k);
    float g = in[i + 1] / (in[i + 1] + average_g / k);
    float b = in[i + 2] / (in[i + 2] + average_b / k);
    out[i] = (unsigned char)(255.0f * r);
    out[i + 1] = (unsigned char)(255.0f * g);
    out[i + 2] = (unsigned char)(255.0f * b);
  }
}
