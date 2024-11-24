# 在InitGPU过程中

创建ImmediateContext。在初始化ImmediateContext的时候，为ImmediateContext创建2个command buffer。此时这2个 command buffer的状态一个是Submitted，一个是NotAllocated，两者都在使用中。

创建ComputeContext也是一样

# 在创建swapchain的过程中

清空renderingBackBuffer，需要用upload command buffer。此时在使用中的两个command buffer的状态分别是Submitted和HasBegin。只有当状态为ReadyForBegin或者NeedReset时，才能被挑选为upload command buffer。因此此时需要创建一个新的command buffer，将它选为upload command buffer，并开始它。





