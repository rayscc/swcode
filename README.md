# swcode
2-PASS COMPRESSION ALGORITHM USING RLC. FOR BINARY IMAGES

**> swcode中一些原创算法的解释文档保存在在doc文件夹(不断完善中)**


### 测试结果

> Intel(R) Core(TM) i5-8250U CPU @ 1.6GHz 1.8GHz,MEM-8GB


|  文件名 | 文件大小(字节) | 压缩后(字节) | 压缩率 | 平均耗时(ms) | 还原测试 | 破坏性测试 |
|:----:|:----:|:----:|:----:|:----:|:----:|:----:|
| VIDEO-1-120-60.bin  | 651600     |144570   |22.19% | 58.86 | <font color=green><b>PASS</b><font> | <font color=red><b>FAILED</b><font>|
| VIDEO-2-120-48.bin  | 432720     |184030   |42.53% | 39.19 | <font color=green><b>PASS</b><font> | <font color=red><b>FAILED</b><font>|
| VIDEO-3-120-60.bin  | 2240100    |652133   |29.11% | 235.30| <font color=green><b>PASS</b><font> | <font color=red><b>FAILED</b><font>|

<font size=2 color=red>备注：每个测试的平均耗时是算法独立重复运行100次后求平均的结果。</font><br><font size=2 color=yellow>还原测试：对压缩后的文件进行还原操作,检测还原文件是否与原文件相同。</font><br><font size=2 color=blue>破坏性测试：对SWC格式文件随机删除或添加任意字符后,仍能保留并正确读取文件大部分信息。</font>


### TODO

由于**swcode**是基于最短游程的编码算法，其对于文件的压缩率高低非常依赖目标文件的内容。这就导致了会出现压缩膨胀的情况，目前的解决方案是通过检测跳变次数预估计文件压缩率，若超过某一阈值，则不对文件进行压缩。但这种方式却直接了降低了**swcode**的破坏性测试通过率~(urays暂时没有想到什么好的解决方式, urays@foxmail.com)