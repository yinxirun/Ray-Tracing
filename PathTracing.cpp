#include "PathTracing.h"

#include <time.h>

#include <fstream>

extern const float PI;
extern const float E;
extern const float epsilon;
extern int hitnum;

unsigned multi_reflect;

PathTracing::PathTracing(Scene *s, Camera *c, unsigned _spp, float _p)
    : scene(s), camera(c), spp(_spp), p(_p), bvh(nullptr) {}

bool PathTracing::enable_BVH() {
  bvh = new BVH();
  bvh->construct_from_mesh(scene->mesh);
  return true;
}

void PathTracing::render() {
  for (unsigned i = 0; i < camera->height; ++i) {
    std::cout << i << " ";

    for (unsigned j = 0; j < camera->width; ++j) {
      Ray ray = emit(j, i);

      Vector3D color;
      for (unsigned sample = 0; sample < spp; ++sample) {
        multi_reflect = 0;
        Vector3D radiance = trace(ray);
        color = color + radiance;
      }
      color = color / spp;
      camera->image[(i * camera->width + j) * 3] = color.z;
      camera->image[(i * camera->width + j) * 3 + 1] = color.y;
      camera->image[(i * camera->width + j) * 3 + 2] = color.x;
    }
    if ((i + 1) % 16 == 0) {
      std::cout << std::endl;
      std::string name = "./examples/" + std::to_string(i) + ".bmp";
      camera->save_BMP(name.c_str());
    }
  }
}

Vector3D PathTracing::trace(Ray ray) {
  //if (multi_reflect >= 5) return Vector3D(0, 0, 0);
  multi_reflect += 1;
  // intersection test
  float t = __FLT_MAX__;
  unsigned face_id = UINT_MAX;
  Material *mtl = nullptr;
  if (bvh == nullptr) {
    for (int i = 0; i < scene->mesh->num_faces(); ++i) {
      float _t = scene->mesh->get_faces()[i].intersect(ray);
      if (_t == __FLT_MIN__) continue;
      if (_t < t) {
        t = _t;
        face_id = i;
      }
    }
    if (t == __FLT_MAX__) return Vector3D(0, 0, 0);
  } else if (!bvh->intersect(ray, bvh->rootNode, t, face_id)) {
    multi_reflect -= 1;
    return Vector3D(0, 0, 0);
  }
  mtl = scene->mesh->get_faces()[face_id].material;

  // determine whether this surface is light source
  bool flag = false;
  for (int i = 0; i < scene->light_sources.size(); ++i) {
    std::vector<unsigned> &light_faces = scene->light_sources[i]->faces_id;
    for (int j = 0; j < light_faces.size(); ++j) {
      if (light_faces[j] == face_id) {
        flag = true;
        break;
      }
    }
    if (flag == true) break;
  }
  if (flag == true) {
    multi_reflect -= 1;
    return mtl->emission;
  }

  // Russian Roulette. Determine whether to reflection.
  float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
  if (r <= p) {
    Vector3D point_hit = ray.origin + t * ray.direction;
    Vector3D normal = scene->mesh->get_faces()[face_id].normal_average();

    //  随机地反射光线（间接光）
    Vector3D direction_new = Vector3D::random();
    while (Vector3D::dot(direction_new, normal) <= epsilon) {
      direction_new = Vector3D::random();
    }
    direction_new.normalize();
    Ray ray_new(point_hit, direction_new);
    //  递归地求得反射光辐射率

    Vector3D indirect_illum = trace(ray_new) / p;

    Vector3D radiance0 = 2 * PI * (Vector3D::dot(direction_new, normal)) *
                         indirect_illum *
                         BRDF(-direction_new, -ray.direction, mtl, normal);

    // 朝光源反射光线（直接光）
    Vector3D radiance1(0, 0, 0);
    float num = 0;
    for (int i = 0; i < scene->light_sources.size(); ++i) {
      std::vector<unsigned> &light_faces = scene->light_sources[i]->faces_id;
      for (int j = 0; j < 1; ++j) {
        // 从众多面光源中随机选一个
        unsigned index = (unsigned)(rand() % light_faces.size());
        index = light_faces[index];
        Triangle &light = scene->mesh->get_faces()[index];
        // 随机选面光源上的一点
        Vector3D point_onlight = light.centre();

        // 构造入射光线
        direction_new = point_onlight - point_hit;
        // 检查反射点和光源之间是否有遮挡
        if (Vector3D::dot(direction_new, light.normal_average()) >= -epsilon)
          continue;
        if (Vector3D::dot(direction_new, normal) <= epsilon) continue;
        direction_new.normalize();
        ray_new.assign(point_hit, direction_new);
        float t_temp;
        unsigned faceid_temp;
        bool outcome =
            bvh->intersect(ray_new, bvh->rootNode, t_temp, faceid_temp);
        if (outcome && faceid_temp == index) {
          hitnum += 1;
          // 如果没有遮挡，则计算渲染方程
          Vector3D dist = point_onlight - point_hit;
          float square_dist =
              dist.x * dist.x + dist.y * dist.y + dist.z * dist.z;
          Vector3D contri =
              (-Vector3D::dot(direction_new, light.normal_average())) *
              (Vector3D::dot(direction_new, normal)) * light.area() *
              light.material->emission *
              BRDF(-direction_new, -ray.direction, mtl, normal) / square_dist;
          radiance1 = radiance1 + contri;
          break;
        }
      }
    }

    // ---------------------------------------------------
    Vector3D ret = radiance0 + radiance1 + mtl->emission;
    multi_reflect -= 1;
    return ret;
  } else {
    multi_reflect = -1;
    return mtl->emission;
  }
}

Ray PathTracing::emit(unsigned x, unsigned y) {
  Vector3D direction;
  float h = (float)camera->height;
  float w = (float)camera->width;
  float alpha = w / h;
  float fovy = camera->fov / 180 * PI;
  Vector3D right = Vector3D::cross(camera->lookat, camera->up);
  direction = (-alpha + alpha / w + (float)x * 2 * alpha / w) * right;
  direction = direction + (-1 + 1 / h + (float)y * 2 / h) * camera->up;
  direction = direction + (1 / tan(fovy / 2)) * camera->lookat;
  direction.normalize();
  Ray r(camera->location, direction);
  return r;
}

Vector3D PathTracing::BRDF(Vector3D inc, Vector3D refl, Material *mtl,
                           Vector3D n) {
  Vector3D h = (refl - inc);
  h.normalize();
  Vector3D s = (pow(Vector3D::dot(n, h), mtl->shineness) * mtl->specular);
  return mtl->diffuse + s;
}