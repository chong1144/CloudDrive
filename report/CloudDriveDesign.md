# 网盘功能的设计
<p align='right'><b>1752877 胡轩</b></p>
<p align='right'><b>1752132 王森</b></p>
<p align='right'><b>1752910 张钇文</b></p>
<!-- @import "[TOC]" {cmd="toc" depthFrom=1 depthTo=6 orderedList=false} -->

<!-- code_chunk_output -->

- [网盘功能的设计](#网盘功能的设计)
  - [总体架构图](#总体架构图)
  - [数据库设计](#数据库设计)
  - [储存方案设计](#储存方案设计)
  - [用户目录设计](#用户目录设计)
  - [通讯协议的设计](#通讯协议的设计)
  - [`Windows客户端` 服务器的设计](#windows客户端-服务器的设计)
  - [`Windows客户端` 设计](#windows客户端-设计)
  - [`Web客户端` 服务器设计](#web客户端-服务器设计)
  - [`Web客户端` 设计](#web客户端-设计)

<!-- /code_chunk_output -->


## 一、总体架构图



## 二、数据库设计

## 三、储存方案设计

## 四、用户目录设计

## 五、通讯协议的设计

### 控制 请求包
#### 注册
+ c-->s
+ s-->c
#### 登录
#### 心跳
#### 同步目录
#### 上传
##### 发送方
+ 文件名
+ 大小
+ 路径


#### 下载
#### 操作请求
+ 新建文件夹
+ 删除文件、文件夹
+ 移动文件
+ 重命名
### 传输
接收方请求

###  



## 六、`Windows客户端` 服务器的设计

### 上传模块交互
	控制模块接收到上传请求后，

## 七、`Windows客户端` 设计

## 八、`Web客户端` 服务器设计

## 九、`Web客户端` 设计




# 网盘功能的设计
<p align='right'><b>1752877 胡轩</b></p>
<p align='right'><b>1752132 王森</b></p>
<p align='right'><b>1752910 张钇文</b></p>



<!-- @import "[TOC]" {cmd="toc" depthFrom=2 depthTo=6 orderedList=false} -->

<!-- code_chunk_output -->

- [网盘功能的设计](#网盘功能的设计)
  - [总体架构图](#总体架构图)
  - [数据库设计](#数据库设计)
  - [储存方案设计](#储存方案设计)
  - [用户目录设计](#用户目录设计)
  - [通讯协议的设计](#通讯协议的设计)
    - [概述](#概述)
    - [设计框图](#设计框图)
      - [类与类间关系](#类与类间关系)
      - [解释说明](#解释说明)
    - [使用方法](#使用方法)
  - [`Windows客户端` 服务器的设计](#windows客户端-服务器的设计)
  - [`Windows客户端` 设计](#windows客户端-设计)
  - [`Web客户端` 服务器设计](#web客户端-服务器设计)
  - [`Web客户端` 设计](#web客户端-设计)

<!-- /code_chunk_output -->


## 总体架构图

## 数据库设计

## 储存方案设计

## 用户目录设计

## 通讯协议的设计
### 概述
本次的设计协议主要通过结构体进行 `socket` 上的传输。
结构体主要分为两类。
- 第一类被称为 `UniformHeader`，功能是告知双方要进行的类型操作，并且告知需要读取多少字节的类型。
- 第二类以 `*Body` 的进行命名。主要就是具体的命令的特定的数据成员。因不同的结构体而异。
### 设计框图
#### 类与类间关系
![communicate](img/communicate.png)

#### 解释说明
本通讯协议由 `Header + Body` 模式进行传送。即每次通过先通过传送统一大小、统一格式内容的 `Header` 来告知接收者接下来要传递的包的种类和长度。然后在读取特定长度的包，进行信息的通知。

上图展示了基本的使用方法和类内关系。
>eg: 如果我们想要发送 `登陆请求`，需要先发送 `UniformHeader`,告知接收方下一个包是 `SigninBody`。

类内的第一行是类（数据包）的名称。接下来的行指向的是数据成员，由 `名称：类型` 组成。
> eg: SigninBody这个数据包由以下三个数据成员： 
> - Username 用户名 `char[]`
> - Password 密码 `char[]`
> - Session 建立的会话 `char[]`

### 使用方法

根据使用场景不同，将使用场景分为以下几种。
- 登陆用户
> 1. `UniformHeader` + `SigninBody`    `C->S`
> 2. `UniformHeader` + `SigninresBody` `S->C`


- 注册用户
> 1. `UniformHeader` + `SignupBody` `C->S`
> 2. `UniformHeader` + `SignupresBody` `S->C`		

- 上传数据
> 1. `UniformHeader` + `UploadReqBody` `C->S`
> 2. `UniformHeader` + `UploadRespBody` `S->C`
> 3. `UniformHeader` + `UploadFetchBody` `S->C`
> 4. `UniformHeader` + `UploadPushBody` `C->S`

- 下载数据
> 1. `UniformHeader` + `DownloadReqBody` `C->S`
> 2. `UniformHeader` + `DownloadRespBody` `S->C`
> 3. `UniformHeader` + `DownloadPushBody` `S->C`

- 显示文件夹内容
> 1. `UniformHeader` + `SYNReqBody` `C->S`
> 2. `UniformHeader` + `SYNRespBody` `S->C`
> 3. `UniformHeader` + `SYNPushBody` `S->C`

## `Windows客户端` 服务器的设计

## `Windows客户端` 设计

## `Web客户端` 服务器设计

## `Web客户端` 设计
