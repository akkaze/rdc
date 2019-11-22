# RDC
Reliable Distributed Communication  
RDC-CI Status: [![Travis-CI Status](https://travis-ci.org/akkaze/rdc.svg?branch=master)](https://travis-ci.org/akkaze/rdc)  
RDC 是一个实验性质的基于C++的通用分布式计算框架。RDC采用MPI类型的接口，但是旨在提供MPI不提供的容错以及动态节点增删功能。RDC使用共享内存，RDMA（包括Infiniband跟Roce两种硬件实现），tcp以及cuda（未来提供支持）作为 通信 后端，使用checkpoint机制来提供容错跟动态节点功能。  
### RDC的可能应用场景  
1. 分布式机器学习，例如同步或者异步SGD。  
2. HPC，例如分子动力学模拟，第一性原理计算，有限元模拟等各学科的超算场景。  
## 安装  
首先在终端运行`pip install -r requirements.txt`  
RDC使用scons来编译c++，所以需要安装scons，scons是一个用python编写的构建工具，可以使用pip安装，在终端运行`pip install --egg scons`  
然后使用脚本安装`chmod +x install.sh && ./install.sh`  
#### 一个非常简单的两进程通信程序  
```python
#!/usr/bin/env python
import rdc
import numpy as np

rdc.init()
comm = rdc.new_comm('main')
if rdc.get_rank() == 0:
    s = 'hello'
    buf = rdc.Buffer(s.encode())
    comm.send(buf, 1)
elif rdc.get_rank() == 1:
    s = '00000'
    buf = rdc.Buffer(s.encode())
    comm.recv(buf, 0)
    print(buf.bytes())
rdc.finalize()
```
将以上代码保存为 sendrecv.py。  

RDC提供tracker来启动节点并监视节点的。提供三种launcher来启动分布式程序，分别为local launcher，tmux launcher（local的扩展，主要为了调试）以及ssh launcher。  

要在本地运行该程序，需要两个启动进程。 在终端输入 `python -m tracker.lancher_local -n 2 sendrecv.py` 即可。  
如果要以ssh方式运行，需要一个配置文件，最简单的配置文件如下  
```python
[common]
init='source ~/.zshrc'
[localhost:1]
[localhost:2]
```
使用ini格式，common表示当前section对所有节点有效，common下面输出节点的ip，格式为[ip:rank]  
将上面的文件保存为common.ini。在终端输入 `python -m tracker.lancher_ssh -f common.ini sendrecv.py` 即可。  
