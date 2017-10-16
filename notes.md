# GStreamer

## History
* 2017/10/16 init

## Build GStreamer with cerbero
```
$ git clone git://anongit.freedesktop.org/gstreamer/cerbero
$ ./cerbero-uninstalled -c config/xxx.cbc bootstrap
```
* bootstrap : 下载并安装当前环境下编译时所需工具
* build : 编译某个recipe，包括其依赖项
* buildone : 编译某个recipe，不包括其依赖项
* cleanone : 清理某个recipe
* wipe : Wipes everything to restore the build system
```
$ ./cerbero-uninstalled -c config/xxx.cbc package gstreamer-1.0
```
reference:
* Building from source using Cerbero [link](https://gstreamer.freedesktop.org/documentation/installing/building-from-source-using-cerbero.html)
* GStreamer SDK 1.0 Build Via Cerbero [link](http://www.jianshu.com/p/7863404c1909)
