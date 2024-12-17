[[vk::binding(0, 0)]] StructuredBuffer data;

[numthreads(128, 1, 1)] void main(uint3 groupThreadId : SV_GROUPTHREADID)
{
    uint index = groupThreadId.x
    data[index] = data[index] * data[index];
}