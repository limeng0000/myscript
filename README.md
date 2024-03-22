# myscript

#### 介绍

myscript是用c语言开发的一个动态强类型的脚本语言，语法是JavaScript子集。

设计目标有以下几个方面：

1. 脚本语言

   myscript是一个动态的脚本语言，用一个c语言写的解释器，解释执行，体积超小。

2. 轻量级

   myscirpt源码将控制在5000行以内的c语言代码，因为代码量非常小，现所有代码写在一个c文件中，方便嵌入到其他的程序中去或者独立运行。代码注释基本完善，满足使用者阅读源码需求，利于在myscript基础之上实现个性化定制。

3. 易用性

   myscript语言是JavaScript子集，在有javascript相关开发经验上可迅速上手，并不需要学习新的编程语法，即使没有JavaScript经验，myscript语法也足够简单，容易上手。另外，虽然myscript使用javascript语法作为模板开发，但是设计思路和细节和javascript并不相同，比如JavaScript实现一个for循环有n种写法，而myscript只提供两种，之所以这样是因为，一方面要源代码尽量精简，另一方面，也是主要的方面是通常开发的时候并不需要“茴”的n种写法，只有一种已经满足开发需求了，毕竟“less is more”

4. 面向对象

   myscript是面向对象的一种编程语言，这点和javascript基于原型来实现面向对象不同，相关的实现参照了python的面向对象，不过只能单继承。


#### 软件架构

myscript没有设计上的架构，重构就是架构。

#### 安装教程

1. 从git上拉取代码

   如果没有安装git，可以直接复制myscript.c和demo.js文件到本地目录

2. 编译代码

   进入下载后的目录，使用gcc编译
   
   ```
   gcc myscript.c
   ```

   编译后：

   linux会生成a.out的可执行文件，执行a.out

   window会生成a.exe的可执行文件，执行a.exe

2. Gitee 官方博客 [blog.gitee.com](https://blog.gitee.com/)
3. 你可以 https://gitee.com/explore 这个地址来了解 Gitee 上的优秀开源项目
4. [GVP](https://gitee.com/gvp) 全称是 Gitee 最有价值开源项目，是综合评定出的优秀开源项目
5. Gitee 官方提供的使用手册 https://gitee.com/help
6. Gitee 封面人物是一档用来展示 Gitee 会员风采的栏目 https://gitee.com/gitee-stars/
