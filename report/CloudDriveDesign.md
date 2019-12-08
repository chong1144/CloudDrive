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
- 登录用户
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
### 控制模块
控制模块负责监听端口并与客户端建立连接。控制模块接收client发来的所有“命令”，如注册、登录、同步用户目录、修改目录、上传和下载等。其中，注册、登录、同步用户目录和修改目录四个命令，由控制模块联合数据库访问模块完成。控制模块收到上传命令并解析完毕后，将具体的文件名、用户等信息交给上传模块，具体上传任务由receiver模块配合数据库模块、文件读写模块完成。控制模块收到下载命令并解析完毕后，将具体的文件名、用户等信息交给下载模块，具体下载任务由sender模块配合数据库模块、文件读写模块完成。

### receiver 模块
receiver模块负责与客户端的sender模块通过TCP连接交互，完成上传任务。
receiver模块的工作流程如图所示：
![receiver](img/receiver.bmp)
#### 解释说明
从控制模块收到用户id和文件信息后，receiver模块维护一个“文件-socket”表，将正在上传该文件的用户socket放一个集合中。receiver模块使用文件hash查询文件信息，查询结果分为3类。若文件存在且完整，就完成上传，即“秒传”。若文件不存在则在数据库Files表中新增文件信息。若文件存在但不完整，则取标记文件块的位示图，从中选取未上传的文件块，并指派一个空闲的socket发送取该文件块的请求，然后开始接收文件上传。接收的同时将文件发送给文件读写模块写入磁盘。若接收了完整的一块文件，则更新位示图。
文件名和存储位置映射关系的建立过程，完成上传后修改“文件-socket”表的过程，和文件读写模块交互的详细过程未在图中展示。

### sender 模块
sender模块负责与客户端的receiver模块通过TCP连接交互，完成下载任务。
![sender](img/sender.bmp)

#### 解释说明

从控制模块收到用户id和文件信息后，sender模块维护一个“socket-文件”表，表中建立socket和文件的单一映射关系，即每个socket对应一个文件，直到完成下载。客户端的receiver模块维护服务端receiver模块的位示图结构，当socket空闲时向服务端sender模块发送指定的接收块号。sender模块访问数据库获得文件标识，结合客户端发来的指定块号向文件读写模块发送读取请求。sender模块读到文件内容的同时向该客户端发送文件数据。

## `Windows客户端` 设计

## `Web客户端` 服务器设计

## `Web客户端` 设计
