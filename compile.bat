cd shaders

glslang -V a.vert -o a.vert.spv --target-env vulkan1.0
glslang -V a.frag -o a.frag.spv --target-env vulkan1.0

glslang -V test.comp -o test.comp.spv --target-env vulkan1.0

@REM glslc a.vert -o a.vert.spv
@REM glslc a.frag -o a.frag.spv

cd ..