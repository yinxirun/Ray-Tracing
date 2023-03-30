#include "PathTracing.h"

#include <ctime>
#include <fstream>

extern const float PI;
extern const float E;
extern const float epsilon;
extern int hitnum;
int num_reflection;
int max_reflection;
float max_radiance;

PathTracing::PathTracing(Scene *s, Camera *c, unsigned _spp, float _p)
    : scene(s), camera(c), spp(_spp), p(_p), bvh(nullptr) {}

bool PathTracing::enable_BVH() {
  bvh = new BVH();
  bvh->construct_from_mesh(scene->mesh);
  std::cout << "BVH has been constructed. " << std::endl;
  return true;
}

void PathTracing::render() {
  float *temp_image = new float[camera->height * camera->width * 3];
  for (unsigned i = 0; i < camera->height * camera->width * 3; ++i)
    temp_image[i] = 0;

  for (unsigned sample = 0; sample < spp; ++sample) {
    max_reflection = INT_MIN;
    max_radiance = __FLT_MIN__;
    num_reflection = 0;
    clock_t start = clock();
    for (unsigned i = 0; i < camera->height; ++i) {
      for (unsigned j = 0; j < camera->width; ++j) {
        Ray ray = emit(j, i);
        // Vector3D radiance = trace(ray);
        Vector3D radiance;
        float t;
        unsigned face_id;
        bool hit = bvh->intersect(ray, bvh->rootNode, t, face_id);
        if (hit) {
          // 是否击中光源
          bool flag = false;
          for (int i = 0; i < scene->light_sources.size(); ++i) {
            std::vector<unsigned> &light_faces =
                scene->light_sources[i]->faces_id;
            for (int j = 0; j < light_faces.size(); ++j) {
              if (light_faces[j] == face_id) {
                flag = true;
                break;
              }
            }
            if (flag == true) break;
          }
          // 如果击中的不是光源
          Vector3D hit_point = ray.origin + t * ray.direction;
          Vector3D hit_normal = scene->mesh->get_faces()[face_id].face_normal;
          Material *mtl = scene->mesh->get_faces()[face_id].material;

          if (!flag)
            radiance =
                shade(hit_point, hit_normal, mtl, -ray.direction, face_id);
          else
            radiance = mtl->emission;
        }

        temp_image[(i * camera->width + j) * 3] += radiance.z;
        temp_image[(i * camera->width + j) * 3 + 1] += radiance.y;
        temp_image[(i * camera->width + j) * 3 + 2] += radiance.x;
      }
    }
    for (unsigned i = 0; i < camera->height * camera->width * 3; ++i) {
      camera->image[i] = temp_image[i] / (sample + 1);
      if (camera->image[i] > max_radiance) max_radiance = camera->image[i];
    }
    clock_t end = clock();
    std::cout << "MAX RADIANCE " << max_radiance << std::endl;
    std::cout << "MAX NUMBER REFLECTION " << max_reflection << std::endl;
    std::cout << "TIME " << (end - start) / 1000 << " second. " << std::endl;
    std::string name = "./examples/" + std::to_string(sample) + ".bmp";
    camera->save_BMP(name.c_str());
  }
  delete[] temp_image;
}

Vector3D PathTracing::shade(Vector3D point, Vector3D normal, Material *m,
                            Vector3D wo, unsigned id) {
  num_reflection += 1;
  if (num_reflection > max_reflection) max_reflection = num_reflection;

  if (m->map != NULL)
    m->diffuse = scene->mesh->faces[id].texture_mapping(point);

  // indirect illumination
  Vector3D indirect_radiance(0, 0, 0);
  float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
  if (r < p ) {
    Vector3D wi = Vector3D::random();
    wi.normalize();
    while (Vector3D::dot(wi, normal) >= -epsilon) {
      wi = Vector3D::random();
    }
    float t;
    unsigned face_id;
    bool hit = bvh->intersect(Ray(point, -wi), bvh->rootNode, t, face_id);

    if (hit) {
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
      // 如果击中的不是光源
      Vector3D hit_point = point - t * wi;
      Vector3D hit_normal = scene->mesh->get_faces()[face_id].face_normal;
      Material *mtl = scene->mesh->get_faces()[face_id].material;
      if (!flag) {
        indirect_radiance = (Vector3D::dot(-wi, normal)) *
                            shade(hit_point, hit_normal, mtl, wi, face_id) *
                            BRDF(wi, wo, m, normal) / p;
      }
    }
  }

  // direct illumination
  Vector3D direct_radiance(0, 0, 0);
  for (int i = 0; i < scene->light_sources.size(); ++i) {
    std::vector<unsigned> &light_faces = scene->light_sources[i]->faces_id;
    // for (int j = 0; j < light_faces.size(); ++j) {
    for (int j = 0; j < 1; ++j) {
      int index = rand() % light_faces.size();
      Triangle &light = scene->mesh->get_faces()[light_faces[index]];
      // 随机选面光源上的一点
      Vector3D point_onlight = light.center;

      // 构造入射光线
      Vector3D wi = point - point_onlight;
      // 检查反射点和光源之间是否有遮挡
      wi.normalize();
      if (Vector3D::dot(wi, light.face_normal) <= epsilon) continue;
      if (Vector3D::dot(-wi, normal) <= epsilon) continue;
      float t_temp;
      unsigned faceid_temp;
      bool outcome =
          bvh->intersect(Ray(point, -wi), bvh->rootNode, t_temp, faceid_temp);
      if (outcome && faceid_temp == light_faces[j]) {
        hitnum += 1;
        // 如果没有遮挡，则计算渲染方程
        Vector3D dist = point_onlight - point;
        float square_dist = dist.x * dist.x + dist.y * dist.y + dist.z * dist.z;
        Vector3D contri = (Vector3D::dot(wi, light.face_normal)) *
                          (Vector3D::dot(-wi, normal)) * light.area() *
                          light.material->emission / square_dist *
                          BRDF(wi, wo, m, normal);
        direct_radiance = direct_radiance + contri;
      }
    }
  }
  num_reflection -= 1;
  return direct_radiance + indirect_radiance;
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
  return (mtl->diffuse + s);
}