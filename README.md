#Proyecto Sistemas Operativos: Interprete de comandos (Shell)

Este proyecto consiste en la implementación de un intérprete de comandos (shell) como parte del curso de Sistemas Operativos. La shell desarrollada permite ejecutar varios comandos típicos de sistemas operativos basados en Unix/Linux.

##Integrantes:
-Oscar Castillo
-Gaspar Jimenez
-Matias Gayoso
-Pedro Muñoz

##Instrucciones de uso:
1. Clonar repositorio
```bash
git clone https://github.com/ocastillo200/Tarea1SO.git
```
2. Actualizar paquetes
```bash
sudo apt update
sudo apt install build-essential
```
3. Compilar el código (verificar que se está situado en la carpeta del repositorio)
```bash
g++ shell.cpp
```
4. Ejecutar el programa
```bash
./a.out
```
5. Opcional (en caso de errores de ejecución):
Si el programa no realiza de manera correcta los comandos de creación de archivos como favs --crear, quizás sea necesario ejecutarlo con permisos adicionales de la siguiente forma:
```bash
sudo ./a.out
```

