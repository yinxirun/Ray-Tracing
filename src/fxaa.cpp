#include "fxaa.h"

using namespace glm;

// 最小阈值，参考值——1/32（visible limit），1/16（high quality），1/12（upper
// limit，start of visible unfiltered edges）
const float EDGE_THRESHOLD_MIN = 0.0312;

// 对比度检测阈值，参考值——1/3（too little），1/4（low quality），1/8（high
// quality），1/16（overkill）
const float EDGE_THRESHOLD = 0.125;

const int ITERATIONS = 12;

float QUALITY(int i)
{
  return 1.0f;
  float arr[] = {1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0};
  if (i < 5)
    return 1.0;
  else if (i < 12)
    return arr[i - 5];
  else
    return 8.0;
}

#define SUBPIXEL_QUALITY 0.75

float clamp(float x, float mi, float ma)
{
  return std::min(ma, std::max(mi, x));
}

float rgb2luma(vec3 rgb)
{
  // return rgb.y * (0.587 / 0.299) + rgb.x;
  return sqrt(dot(rgb, vec3(0.299, 0.587, 0.114)));
}

vec3 FXAA::texture(std::vector<vec3> &tex, ivec2 xy)
{
  int x = std::min(width - 1, std::max(0, xy.x));
  int y = std::min(height - 1, std::max(0, xy.y));
  int index = x + y * width;
  return tex[index];
}

vec3 FXAA::texture(std::vector<vec3> &tex, vec2 uv)
{
  float x = width * uv.x;
  float y = height * uv.y;

  int x1 = (int)(x - 0.5);
  int x2 = (int)(x + 0.5);
  int y1 = (int)(y - 0.5);
  int y2 = (int)(y + 0.5);

  vec3 c0 = texture(tex, ivec2(x1, y1));
  vec3 c1 = texture(tex, ivec2(x2, y1));
  vec3 c2 = texture(tex, ivec2(x1, y2));
  vec3 c3 = texture(tex, ivec2(x2, y2));

  vec3 t1 = (1 - y + y1 + 0.5f) * c0 + (y - y1 - 0.5f) * c2;
  vec3 t2 = (1 - y + y1 + 0.5f) * c1 + (y - y1 - 0.5f) * c3;

  return (1 - x + x1 + 0.5f) * t1 + (x - x1 - 0.5f) * t2;
}

std::vector<vec3> FXAA::operator()(std::vector<vec3> &input, unsigned width,
                                   unsigned height)
{
  pixels.resize(width * height);
  this->width = width;
  this->height = height;

  vec2 inverseScreenSize(1.0f / width, 1.0f / height);

  for (int i = 0; i < height; ++i)
  {
    for (int j = 0; j < width; ++j)
    {
      vec2 uv(float(j + 0.5) / width, float(i + 0.5) / height);

      vec3 colorCenter = texture(input, ivec2(j, i));

      // 当前像素的亮度
      float lumaCenter = rgb2luma(colorCenter);

      // 跟当前像素直接相邻的四个像素的亮度
      float lumaDown = rgb2luma(texture(input, ivec2(j, i + 1)));
      float lumaUp = rgb2luma(texture(input, ivec2(j, i - 1)));
      float lumaLeft = rgb2luma(texture(input, ivec2(j - 1, i)));
      float lumaRight = rgb2luma(texture(input, ivec2(j + 1, i)));

      // 找到当前像素周围最大和最小的亮度
      float lumaMin = std::min(
          lumaCenter,
          std::min(std::min(lumaDown, lumaUp), std::min(lumaLeft, lumaRight)));
      float lumaMax = std::max(
          lumaCenter,
          std::max(std::max(lumaDown, lumaUp), std::max(lumaLeft, lumaRight)));

      // 计算最大亮度和最小亮度的差
      float lumaRange = lumaMax - lumaMin;

      // 如果亮度变化低于阈值（或者在一个非常黑暗的区域），像素就不在边缘上，不执行任何AA。
      if (lumaRange < std::max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD))
      {
        pixels[i * width + j] = colorCenter;
        continue;
      }

      // 查询剩余的4个角的亮度。
      float lumaDownLeft = rgb2luma(texture(input, ivec2(j - 1, i + 1)));
      float lumaUpRight = rgb2luma(texture(input, ivec2(j + 1, i - 1)));
      float lumaUpLeft = rgb2luma(texture(input, ivec2(j - 1, i - 1)));
      float lumaDownRight = rgb2luma(texture(input, ivec2(j + 1, i + 1)));

      // 合并四条边的亮度（使用中间变量为未来计算相同的值）。
      float lumaDownUp = lumaDown + lumaUp;
      float lumaLeftRight = lumaLeft + lumaRight;

      // 角落同样的处理
      float lumaLeftCorners = lumaDownLeft + lumaUpLeft;
      float lumaDownCorners = lumaDownLeft + lumaDownRight;
      float lumaRightCorners = lumaDownRight + lumaUpRight;
      float lumaUpCorners = lumaUpRight + lumaUpLeft;

      // 沿水平和垂直轴向计算梯度的估计值。
      float edgeHorizontal = abs(-2.0 * lumaLeft + lumaLeftCorners) +
                             abs(-2.0 * lumaCenter + lumaDownUp) * 2.0 +
                             abs(-2.0 * lumaRight + lumaRightCorners);
      float edgeVertical = abs(-2.0 * lumaUp + lumaUpCorners) +
                           abs(-2.0 * lumaCenter + lumaLeftRight) * 2.0 +
                           abs(-2.0 * lumaDown + lumaDownCorners);

      // 边是水平的还是垂直的？
      bool isHorizontal = (edgeHorizontal >= edgeVertical);

      // Step 3 选择边的取向

      // 在与边相反的方向上选择两个相邻的纹理亮度。
      float luma1 = isHorizontal ? lumaUp : lumaLeft;
      float luma2 = isHorizontal ? lumaDown : lumaRight;

      // 计算这个方向的梯度。
      float gradient1 = luma1 - lumaCenter;
      float gradient2 = luma2 - lumaCenter;

      // 哪个方向最陡？
      bool is1Steepest = abs(gradient1) >= abs(gradient2);

      // 对应方向的梯度，归一化。
      float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

      // 根据边的方向选择步长（一个像素）。
      float stepLength = isHorizontal ? 1.0 / height : 1.0 / width;

      // 在正确方向上移动像素的亮度到中心像素亮度的局部平均亮度。
      float lumaLocalAverage = 0.0;

      if (is1Steepest)
      {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5 * (luma1 + lumaCenter);
      }
      else
      {
        lumaLocalAverage = 0.5 * (luma2 + lumaCenter);
      }

      // 将UV向正确的方向移动半像素。
      vec2 currentUV = uv;
      if (isHorizontal)
      {
        currentUV.y += stepLength * 0.5;
      }
      else
      {
        currentUV.x += stepLength * 0.5;
      }

      // Step4 第一次迭代探测

      // 在正确的方向上计算偏移量（每个迭代步骤）。
      vec2 offset = isHorizontal ? vec2(inverseScreenSize.x, 0.0)
                                 : vec2(0.0, inverseScreenSize.y);
      // 计算uv以探索边缘的每一个方向。QUALITY值允许我们加快步伐。
      vec2 uv1 = currentUV - offset;
      vec2 uv2 = currentUV + offset;

      // 读取探测段的两端亮度，并且计算亮度到局部平均亮度的
      float lumaEnd1 = rgb2luma(texture(input, uv1));
      float lumaEnd2 = rgb2luma(texture(input, uv2));
      lumaEnd1 -= lumaLocalAverage;
      lumaEnd2 -= lumaLocalAverage;

      // 如果当前某一端的亮度差大于局部梯度，则说明已经达到边缘的一侧。
      bool reached1 = abs(lumaEnd1) >= gradientScaled;
      bool reached2 = abs(lumaEnd2) >= gradientScaled;
      bool reachedBoth = reached1 && reached2;

      // 如果没有到达边缘一侧，我们将继续朝着这个方向进行探测
      if (!reached1)
      {
        uv1 -= offset;
      }
      if (!reached2)
      {
        uv2 += offset;
      }

      // Step5 继续迭代

      // 如果两边都还没有到达，则继续探测。
      if (!reachedBoth)
      {
        for (int i = 2; i < ITERATIONS; i++)
        {
          // 如果没有到达一侧，读取第一个方向的亮度，计算差值。
          if (!reached1)
          {
            lumaEnd1 = rgb2luma(texture(input, uv1));
            lumaEnd1 = lumaEnd1 - lumaLocalAverage;
          }
          // 如果没有到达另一侧，读取相反方向的亮度，计算差值。
          if (!reached2)
          {
            lumaEnd2 = rgb2luma(texture(input, uv2));
            lumaEnd2 = lumaEnd2 - lumaLocalAverage;
          }
          // 如果当前端的亮度差值大于局部梯度，说明我们已经达到了边的一侧。
          // If the luma deltas at the current extremities is larger than the
          // local gradient, we have reached the side of the edge.
          reached1 = abs(lumaEnd1) >= gradientScaled;
          reached2 = abs(lumaEnd2) >= gradientScaled;
          reachedBoth = reached1 && reached2;

          // 如果这一侧没有到达，我们用变量QUALITY继续沿着这个方向探测。
          if (!reached1)
          {
            uv1 -= offset * QUALITY(i);
          }
          if (!reached2)
          {
            uv2 += offset * QUALITY(i);
          }

          // 如果两侧都已经到达了，则停止探测。
          if (reachedBoth)
          {
            break;
          }
        }
      }

      // Step6 评估偏移
      // 计算当前像素到每一个端点的距离。
      float distance1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
      float distance2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);

      // 到哪个方向的端点最近？
      bool isDirection1 = distance1 < distance2;
      float distanceFinal = min(distance1, distance2);

      // Length of edge
      float edgeThickness = (distance1 + distance2);

      // UV偏移：读取离边的端点最近的一侧。
      // distanceFinal / edgeThickness <
      // 0.5；由于越接近端点，UV偏移需要越大，则取负数；+0.5使取值范围为正数。
      float pixelOffset = -distanceFinal / edgeThickness + 0.5;

      // 是否中心亮度小于局部平局亮度？
      bool isLumaCenterSmaller = lumaCenter < lumaLocalAverage;

      // 如果中心的亮度比邻近的小，则两端的亮度差应均为正（变化相同）
      // （在边缘较近的一边的方向。）
      bool correctVariation =
          ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;

      // 如果亮度变化不正确，不要偏移。
      float finalOffset = correctVariation ? pixelOffset : 0.0;

      // Step7 子像素反走样
      // 子像素转移
      // 相邻的3x3像素的加权平均亮度。
      float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaDownUp + lumaLeftRight) +
                                          lumaLeftCorners + lumaRightCorners);
      // 求全局平均亮度和中心亮度的差，和3x3相邻的lumaRange的比值。
      float subPixelOffset1 =
          clamp(abs(lumaAverage - lumaCenter) / lumaRange, 0.0, 1.0);
      float subPixelOffset2 =
          (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
      // 计算基于此差值的子像素偏移量。
      float subPixelOffsetFinal =
          subPixelOffset2 * subPixelOffset2 * SUBPIXEL_QUALITY;

      // 从两个偏移量中选最大偏移量。
      finalOffset = max(finalOffset, subPixelOffsetFinal);

      // Step8 最后
      // 计算最后的UV坐标。
      vec2 finalUv = uv;
      if (isHorizontal)
      {
        finalUv.y += finalOffset * stepLength;
      }
      else
      {
        finalUv.x += finalOffset * stepLength;
      }

      // 在新的UV坐标上读取颜色，并使用它。
      vec3 finalColor = texture(input, finalUv);
      pixels[i * width + j] = finalColor;
    }
  }

  return pixels;
}