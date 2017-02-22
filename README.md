# FastCGI-Framework
基于FastCGI的web开发框架，用于C/C++开发Web程序。

## 目前已实现的功能：
* 一个请求由一个线程处理，线程会预先创建。
* 抽象了数据库访问接口，目前支持MySQL, Tokyotyrant, sphinx
* 页面生成可配置：只需配置即可生成一些典型的页面
* 模板引擎
* 表单处理
* 安全特性：防XSS注入、CSRF、防SQL注入
* 单入口文件、路由解析
* Session
* 验证码生成
* 错误处理
* request和response处理: cookie 缓存控制 referer等

## 注意
* 该项目依赖代码还没有整理出来，目前编译还会提示缺少一些依赖库。
* 该项目打算重新设计、代码重构一下。
* 有问题请反馈：dudubird2006###163.com


## 开发调试fastcgi程序
* 调试：先用spawn-fcgi启动fcgi程序后，用 gdb -p pid来调试（先设断点运行后再发请求），如果不需要调试有请求的情况，可以直接通过 gdb fcgi程序来调试。
* spawn-fcgi的'-n'选项(不fork子进程)可以用于valgrind进行内存泄露检测。
* 检测内存泄露和段错误  
`valgrind --tool=memcheck --leak-check=yes --trace-children=yes  --log-file=valgrind_log spawn-fcgi -s /tmp/fcgi_app.sock -M 511 -P /tmp/fcgi_app.pid -n -- fcgi_app`
