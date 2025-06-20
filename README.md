# Rootkits TEC

# Requisitos

- Ubuntu 20.04 (Necesario por el kernel 5.4)
- vMWare Fusion o VirtualBox
- [Vagrant](https://developer.hashicorp.com/vagrant/docs/installation) 
- Procesador x86_64 (64 bits)

# Instalación y Ejecución

1. Crear VM en Vagrant

```
vagrant init generic/ubuntu2004
vagrant up
vagrant ssh
```

2. Instalar dependencias necesarias

Esto instalará los headers del kernel de Linux necesarios para el código, git para nuestro repositorio
y build-essential para compilar el código.

```
sudo apt update; 
sudo apt install git build-essential linux-headers-$(uname -r) build-essential
```

3. Clonar el repositorio y compilar el código

```
vagrant@ubuntu2004:~/git$ git clone https://github.com/bojackhorseman0309/LinuxKernelRootkits.git
Cloning into 'LinuxKernelRootkits'...
remote: Enumerating objects: 34, done.
remote: Counting objects: 100% (34/34), done.
remote: Compressing objects: 100% (24/24), done.
remote: Total 34 (delta 11), reused 27 (delta 7), pack-reused 0 (from 0)
Unpacking objects: 100% (34/34), 14.91 KiB | 1.66 MiB/s, done.
vagrant@ubuntu2004:~/git$ cd LinuxKernelRootkits/
vagrant@ubuntu2004:~/git/LinuxKernelRootkits$ make
make -C /lib/modules/5.4.0-169-generic/build M=/home/vagrant/git/LinuxKernelRootkits modules
make[1]: Entering directory '/usr/src/linux-headers-5.4.0-169-generic'
  CC [M]  /home/vagrant/git/LinuxKernelRootkits/rootkit.o
  Building modules, stage 2.
  MODPOST 1 modules
  CC [M]  /home/vagrant/git/LinuxKernelRootkits/rootkit.mod.o
  LD [M]  /home/vagrant/git/LinuxKernelRootkits/rootkit.ko
make[1]: Leaving directory '/usr/src/linux-headers-5.4.0-169-generic'
```

4. Crear archivos de prueba

```
vagrant@ubuntu2004:~/git/LinuxKernelRootkits$ mkdir test && cd test
vagrant@ubuntu2004:~/git/LinuxKernelRootkits/test$ touch hola.txt omg.txt .lol .hola_profe.c .torvalds.java
vagrant@ubuntu2004:~/git/LinuxKernelRootkits/test$ ls -la
total 8
drwxrwxr-x 2 vagrant vagrant 4096 Jun 19 23:03 .
drwxrwxr-x 5 vagrant vagrant 4096 Jun 19 23:02 ..
-rw-rw-r-- 1 vagrant vagrant    0 Jun 19 23:03 .hola_profe.c
-rw-rw-r-- 1 vagrant vagrant    0 Jun 19 23:03 hola.txt
-rw-rw-r-- 1 vagrant vagrant    0 Jun 19 23:03 .lol
-rw-rw-r-- 1 vagrant vagrant    0 Jun 19 23:03 omg.txt
-rw-rw-r-- 1 vagrant vagrant    0 Jun 19 23:03 .torvalds.java
```

5. Ejecutar rootkit

```
vagrant@ubuntu2004:~/git/LinuxKernelRootkits/test$ sudo insmod ../rootkit.ko 
vagrant@ubuntu2004:~/git/LinuxKernelRootkits/test$ ls -la
ls: cannot access 'oculto': No such file or directory
ls: cannot access 'oculto': No such file or directory
ls: cannot access 'oculto': No such file or directory
total 8
drwxrwxr-x 2 vagrant vagrant 4096 Jun 19 23:03 .
drwxrwxr-x 5 vagrant vagrant 4096 Jun 19 23:02 ..
-rw-rw-r-- 1 vagrant vagrant    0 Jun 19 23:03 hola.txt
-????????? ? ?       ?          ?            ? oculto
-????????? ? ?       ?          ?            ? oculto
-????????? ? ?       ?          ?            ? oculto
-rw-rw-r-- 1 vagrant vagrant    0 Jun 19 23:03 omg.txt
vagrant@ubuntu2004:~/git/LinuxKernelRootkits/test$ sudo rmmod rootkit 
vagrant@ubuntu2004:~/git/LinuxKernelRootkits/test$ ls -la
total 8
drwxrwxr-x 2 vagrant vagrant 4096 Jun 19 23:03 .
drwxrwxr-x 5 vagrant vagrant 4096 Jun 19 23:02 ..
-rw-rw-r-- 1 vagrant vagrant    0 Jun 19 23:03 .hola_profe.c
-rw-rw-r-- 1 vagrant vagrant    0 Jun 19 23:03 hola.txt
-rw-rw-r-- 1 vagrant vagrant    0 Jun 19 23:03 .lol
-rw-rw-r-- 1 vagrant vagrant    0 Jun 19 23:03 omg.txt
-rw-rw-r-- 1 vagrant vagrant    0 Jun 19 23:03 .torvalds.java
```

# Explicación de la implementación

Tras leer distintas referencias, se tomó [esta implementación](https://xcellerator.github.io/posts/linux_rootkits_06/)
de un rootkit que oculta archivos y directorios para usarlo como base.

Ya que la tarea necesitaba mostrar archivos ocultos, se modificó el código para hacer el inverso.

La lógica consiste en que en Linux es común que los archivos ocultos tengan un
prefijo de un punto (`.`) al inicio de su nombre. 
Esto se da debido a que diferencia de otros sistemas como Windows, los archivos no tienen otra manera de ocultarse en 
sus metadatos y por ende la convención de la mayoría de herramientas es tomar esto como si estuviera "oculto" 
para no ser listado al usuario.

El código lo que hace es ejecutar lo que se llama un "hook" en la función `getdents64` , básicamente este hook 
lo que hace es interceptar las llamadas, dejar que pasen, pero modifican algo en el camino, en este caso el buffer 
retornado al usuario.

Este toma a un `struct` de Linux llamado `linux_dirent64` el cual contiene información sobre los directorios. 
Dentro de este `struct` hay un campo llamado `d_name` que es el nombre del archivo o directorio
y un atributo llamado `d_type` que indica el tipo de archivo, en este caso solo tomamos en cuenta archivos en esta prueba.

La lógica principal sería asi:

```
        // Solo dejar pasar archivos
		 if (current_dir->d_type != DT_REG) {
            offset += current_dir->d_reclen;
            continue;
        }

        /* Comparamos si el primer digito es un punto, ya que
		en Linux es un estandar "ocultar" archivos con el punto */
        if (current_dir->d_name[0] == '.')
        {
            strncpy(current_dir->d_name, "oculto", 6);
            current_dir->d_name[6] = '\0';
        }
```

El bloque de arriba índica que si el tipo de archivo es un archivo regular (`DT_REG`)
y el primer carácter del nombre del archivo es un punto (`.`), indicando que es un archivo "oculto".
Después de eso se le cambia el nombre del archivo a "oculto".

Esto lo que hace básicamente es que al listar los archivos en un directorio con herramientas de usuario normales como `ls`,
los archivos que comienzan con un punto serán reemplazados por el nombre "oculto". Sin embargo,
estos archivos no son modificados de ninguna manera y al eliminar el rootkit, los archivos "volverán" a su nombre original.

# Referencias

https://xcellerator.github.io/posts/linux_rootkits_06/

https://github.com/xcellerator/linux_kernel_hacking/tree/master/3_RootkitTechniques/3.4_hiding_directories

https://github.com/m0nad/Diamorphine

https://github.com/ait-aecid/caraxes/tree/main

https://github.com/ilammy/ftrace-hook