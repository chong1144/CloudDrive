
drop database if exists db1752132;
create database db1752132;

use db1752132;

drop table if exists Users;
create table Users (
`Uid` int(11) not null  AUTO_INCREMENT,
`Uname` varchar(32) not null,
`Password` varchar(128) not null,
`IP` varchar(32),
`Lastlogintime` datetime,
`Createtime` datetime,
PRIMARY KEY (Uid)
);


drop table if exists Files;
create table Files (
`Uid` int(11) not null,
`Filename` varchar(64) not null,
`Path` varchar(128) not null,
`Hash` varchar(128) not null,
`Modtime` datetime,
`Isdir` tinyint
);

drop table if exists FileIndex;
create table FileIndex (
`Hash` varchar(128) not null,
`Size` int(11) not null,
`Refcount` int not null,
`Bitmap` text,
`Complete` tinyint
);

insert into Users 
values(null,"root","ff9830c42660c1dd1942844f8069b74a","127.0.0.1",now(),now());


insert into Files values(1,"test.txt","/","ba1f2511fc30423bdbb183fe33f3dd0f",now(),0);

insert into FileIndex values("ba1f2511fc30423bdbb183fe33f3dd0f",100,0,"0",0);

