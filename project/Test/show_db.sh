#!/bin/bash

user=u1752132
password=u1752132
db=db1752132

show_Users="select * from Users;";
show_Files="select * from Files;";
show_FileIndex="select * from FileIndex;";

mysql -u${user} -p${password}  $db -e "$show_Users";

mysql -u$user -p$password  $db -e "$show_Files";
mysql -u$user -p$password  $db -e "$show_FileIndex";