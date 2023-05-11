# myscript

#### 介绍

myscript是用c语言开发的编程语言，语法是JavaScript子集，目前正处于开发阶段😁，只用于实验及学习目的，如果引用请说明来源，最终是实现一个嵌入式的脚本语言。

设计目标有以下几个方面：

1. 脚本语言

   myscript是一个动态的脚本语言，用一个c语言写的解释器，解释执行，体积超小，方便安装使用。

2. 轻量级

   myscirpt源码将控制在5000行以内的c语言代码(现在有效代码量2000多)，因为代码量非常小，考虑将所有代码写在一个c文件中，方便嵌入到其他的程序中去或者独立运行。后续代码注释将陆续完善，满足使用者阅读源码需求，利于在myscript基础之上实现个性化定制。

3. 易用性

   myscript语言是JavaScript子集，在有javascript相关开发经验上可迅速上手，并不需要学习新的编程语法，即使没有JavaScript经验，myscript语法也足够简单，容易上手。另外，虽然myscript使用javascript语法作为模板开发，但是设计思路和细节和javascript并不相同，比如JavaScript实现一个for循环有n种写法，而myscript只提供两种，之所以这样是因为，一方面要源代码尽量精简，另一方面，也是主要的方面是通常开发的时候并不需要“茴”的n种写法，只有一种已经满足开发需求了，毕竟“less is more”

4. 面向对象

   myscript是面向对象的一种编程语言，这点和javascript基于原型来实现面向对象不同，相关的实现参照了python的面向对象，不过只能单继承。面向对象能够方便扩展myscript语言项目的规模

5. 多线程

   后续考虑加入多线程。。。。


#### 软件架构

myscript没有设计上的架构，重构就是架构。

#### 安装教程

1. 从git上拉取代码

   如果没有安装git，可以直接复制myscript.c和demo.js文件到本地目录

2. 编译代码

   进入下载后的目录，使用gcc编译，如果gcc编译器没有安装，请点击这里安装https://sourceforge.net/projects/mingw/files/

   ```
   gcc myscript.c
   ```

   编译后：

   linux会生成a.out的可执行文件，执行a.out

   window会生成a.exe的可执行文件，执行a.exe

   输出的结果demo.js执行的内容

#### 使用说明

已经实现的功能有

1. 控制语句

   - if .. else if..else 语句
   - while循环
   - for循环
   - for of循环

2. 表达式

   需要注意的是myscript是强类型和javascript的弱类型有区别

3. function函数

   函数的定义只提供一种方式，即

   ```javascript
   function max(a, b) {
       if (a > b) {
           return a;
       } else {
           return b;
       };
   };
   ```

4. class类

   定义与JavaScript相同，不过super是关键字，指向的是实例对应类的父类，当调用父类的构造方法时，如下方式

   ```javascript
   super.constructor(arg);
   ```

5. 数组列表

   列表的赋值只提供一种方式，即

   ```javascript
   mycars=["BMW","Volvo","Saab"]
   ```

6. map对象

   对应的是javascript的对象

   ```javascript
   person = { firstName: "John", lastName: "Doe", age: 50, eyeColor: "blue" };
   person["lastName"]
   ```


相关代码的演示如下，并无实际意义，仅供演示作用

```javascript
class Animal {
    constructor(size) {
        this.size = size;
    };
    speak() {
        print("i am animal,my size is 2");
    };
};
class Dog extends Animal {
    name = "dog";
    constructor(name, size) {
        this.name = name;
        super.constructor(size);
    };
    speak() {
        print("i am Dog ,my size is 3");
    };
};
a = new Animal(5);
a.speak();
d = new Dog("cat", 6);
d.speak();
duck = {
    prop: a,
    speak_gua: ["gua", "gua", "gua"],
    speak_ga: ["ga", "ga", "ga"]
};
function speak(list) {
    for (i of list) {
        print(i);
    };
};
if (duck["prop"].size > 6) {
    speak(duck["speak_gua"]);
} else {
    speak(duck["speak_ga"]);
};
```

输出：

```
i am animal,my size is 2
i am Dog ,my size is 3
ga
ga
ga
```


#### 开发计划

1. try...catch语句

#### 参与贡献

1. Fork 本仓库
2. 新建 Feat_xxx 分支
3. 提交代码
4. 新建 Pull Request

#### 特技

1. 使用 Readme_XXX.md 来支持不同的语言，例如 Readme_en.md, Readme_zh.md
2. Gitee 官方博客 [blog.gitee.com](https://blog.gitee.com/)
3. 你可以 https://gitee.com/explore 这个地址来了解 Gitee 上的优秀开源项目
4. [GVP](https://gitee.com/gvp) 全称是 Gitee 最有价值开源项目，是综合评定出的优秀开源项目
5. Gitee 官方提供的使用手册 https://gitee.com/help
6. Gitee 封面人物是一档用来展示 Gitee 会员风采的栏目 https://gitee.com/gitee-stars/