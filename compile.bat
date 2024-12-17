cd shaders

glslang -V src/a.vert -o a.vert.spv --target-env vulkan1.0
glslang -V src/a.frag -o a.frag.spv --target-env vulkan1.0

glslang -V src/test.comp -o test.comp.spv --target-env vulkan1.0

@REM glslc a.vert -o a.vert.spv
@REM glslc a.frag -o a.frag.spv



cd ..