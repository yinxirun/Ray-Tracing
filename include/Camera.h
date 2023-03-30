#ifndef _CAMERA_H
#define _CAMERA_H
#include <iostream>

#include "Ray.h"
#include "Vec.h"

enum projection_type { orthogonal, perspective };

class Camera {
 public:
  Vector3D location;
  Vector3D lookat;
  Vector3D up;
  float fov;  ///< focal distance
  enum projection_type type;
  unsigned height;
  unsigned width;
  float* image;

  Camera();
  Camera(Vector3D _loc, Vector3D _forward, Vector3D _up, float _fov,
         enum projection_type _type);

  /// Destructor. If image frame is allocated, free it.
  ~Camera();

  void set_type(enum projection_type t);

  void set_fov(float _fov);

  void set_lookat(Vector3D& v);

  void set_location(Vector3D& v);

  void set_up(Vector3D& v);

  /// Set the number of rows and columns of pixels
  void set_image_size(unsigned w, unsigned h);

  void save_BMP(const char* filename);
  
  void tone_mapping(float* in, unsigned char* out);
};

#endif