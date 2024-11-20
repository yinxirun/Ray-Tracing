把资源分为3类：uniform buffer和global。其中global包含combined image sampler、input attachment、storage image、storage buffer 等。还有一种packed资源，暂时不考虑。

每个stage分配一个descriptor set，比如说vertex shader使用set 0，fragment使用set 1。

在每个stage（或者说每个set）中，binding编号从0开始，uniform buffer放在最前面，其次是global中除input attachment之外的资源，最后是input attachment。