checked xp sp2版的pci.sys的逆向

其实很早就弄好了
只是pci bridge的行为有些怪异
于是我就想准备个文档啥的
结果我都写的差不多了
才发现微软自己的网站上有这些文档
于是就把代码打个包放上来了

===================================================
微软的文档在这里
这里是关于pci bridge的
http://www.microsoft.com/whdc/system/bus/PCI/pcibridge-cardbus.mspx

这里是关于subtractive decode的
http://download.microsoft.com/download/5/b/5/5b5bec17-ea71-4653-9539-204a672f11cf/PCIbridge-subtr.doc
===================================================

pci的总线驱动整体上讲有主要实现4个功能
1.枚举总线上的设备..这个大家都知道了..
2.pci的电源管理,这也是pci规范里面的东西没什么特别的
3.pci总线上设备的resource仲裁....这个要占很大比例..而且资源仲裁可以说是windows的pnp架构里面一个最重要的组成部分.
   而这个本身又是一个np-complete的问题.可以看看windows在内核层来解决np-complete的有趣实现
4.pci还提供了若干个interface,

1.枚举设备都很容易.就是一个嵌套的循环去尝试读取config space.读取成功了就找到了一个新设备,然后就创建他
2.电源管理就更容易了..写写config space就成
3.资源仲裁看起来很复杂其实不难.基本都是固定的处理方式...
   各位手头有windows 2000源代码的同学可以看看ntos\arb目录.这里是aribter的实现
   然后是io\pnpres.c这个是pnp的资源分配的实现
   虽然现在windows使用的算法跟2000有很多不同了.不过原理基本是一样的
   基本上讲...这个np-complete的问题可以等同于一个背包问题..
4.若干个interface.其实每个都很简单

实际上直到2003.windows的pci总线驱动都并不完善
可以讲..windows的pci总线驱动严重依赖一个正确实现的bios
并且windows的pci总线驱动对hotplug的支持非常非常的有限

微软宣传说vista的pci总线驱动改善了若干若干问题
不过我没逆向vista的版本.具体就不清楚了...

所以...这个代码如果你想调试..请在2003/xp的环境下调试
不要到vista下面去调试...

另外...pci.sys是个boot driver.你需要一个checked版的ntldr才能使用kdfiles来替换目标系统里面的boot driver
windows自己的checked版ntldr只能over com口.请注意
如果你用windows自己的ntldr.那么你在windbg这里ctrl+break是不起作用的.你可以在选择操作系统的界面上按f10..这样会break into debugger

当然你可以选择使用我写的那个ntldr(打个广告,比windows自己的要好)

kdfiles这样写

    map
    \WINDOWS\system32\DRIVERS\pci.sys
    d:\working\pci\bin\checked\pci.sys


注意第2行的路径...跟内核加载的驱动是不一样的
你不能使用 \Systemroot\system32\drivers\pci.sys 这样的路径

强烈要求使用kdfiles
不要去替换你的debuggee上的pci.sys...
不然系统起不来可别怪我没提醒你哟......
