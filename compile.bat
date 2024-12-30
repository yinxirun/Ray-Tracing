cd shaders

glslang -V GLSL/a.vert -o a.vert.spv --target-env vulkan1.0
glslang -V GLSL/a.frag -o a.frag.spv --target-env vulkan1.0

glslang -V GLSL/test_cube.vert -o test_cube.vert.spv --target-env vulkan1.0
glslang -V GLSL/test_cube.frag -o test_cube.frag.spv --target-env vulkan1.0

glslang -V GLSL/test.comp -o test.comp.spv --target-env vulkan1.0

@REM glslc a.vert -o a.vert.spv
@REM glslc a.frag -o a.frag.spv



cd ..