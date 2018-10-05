# LinuxLinkedLists
[EN] 
Linux kernel module for managing a doubly-linked lists with ghost node -which uses few dynamic memory resources- of integers or chars. It includes an alternative implementation of read() using seq_files.
Edit the Makefile for using chars.

Usage: Compile, load the module, access via cat and echo to /proc/modlist


$ make
$ sudo insmod modlist.ko
$ echo add 4 > /proc/modlist  or  $ echo add Madrid > /proc/modlist
...
$ cat /proc/modlist
$ echo remove 4 > /proc/modlist  or  $ echo remove Madrid > /proc/modlist
$ echo cleanup > /proc/modlist
...


[SP]
Módulo del kernel Linux que permite manejar una lista doblemente enlazada con nodo fantasma -y que usa muy poca memoria dinámica - de enteros o chars.
Edita el MakeFile si quieres usar los chars. Incluye una implementación alternativa de read() usando seq_files.
