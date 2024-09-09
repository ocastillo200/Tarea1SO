#include <vector>
#include <iostream>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <thread>
#include <chrono>

using namespace std;

std::map<int, std::string> favorites;
int next_id = 1;                 // ID del siguiente comando a agregar
std::string favorites_file = ""; // Ruta del archivo de favoritos

void saveFavorites()
{
    if (favorites_file.empty())
        return;
    std::ofstream ofs(favorites_file);
    for (const auto &[id, cmd] : favorites)
    {
        ofs << id << " " << cmd << std::endl;
    }
}

// Convertir vector de strings a vector de char* para execvp
std::vector<char *> convertToCharPointers(const std::vector<std::string> &args)
{
    std::vector<char *> cargs;
    for (const auto &arg : args)
    {
        cargs.push_back(const_cast<char *>(arg.c_str()));
    }
    cargs.push_back(nullptr); // execvp requiere un puntero nulo al final
    return cargs;
}

std::vector<std::string> parseInput(const std::string &input)
{
    std::istringstream iss(input);
    std::vector<std::string> args;
    std::string arg;
    while (iss >> arg)
    {
        args.push_back(arg);
    }
    return args;
}

void executeCommand(const std::vector<std::string> &cmd_args)
{
    if (cmd_args.empty())
        return;

    std::string cmd_str = cmd_args[0];
    for (size_t i = 1; i < cmd_args.size(); ++i)
    {
        cmd_str += " " + cmd_args[i];
    }

    // Verificar si el comando es uno de los comandos favoritos
    bool is_favs_command = (cmd_args[0] == "favs");

    if (!is_favs_command)
    {
        // Verificar si el comando ya está en la lista de favoritos
        bool command_exists = false;
        for (const auto &[id, cmd] : favorites)
        {
            if (cmd == cmd_str)
            {
                command_exists = true;
                break;
            }
        }

        pid_t pid = fork();
        if (pid == 0)
        {
            // Proceso hijo
            auto cargs = convertToCharPointers(cmd_args);
            execvp(cargs[0], cargs.data());
            perror("execvp");
            exit(EXIT_FAILURE);
        }
        else if (pid > 0)
        {
            // Proceso padre
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status))
            {
                if (WEXITSTATUS(status) == 0 && !command_exists)
                {
                    // Agregar a favoritos si el comando no está en la lista y terminó exitosamente
                    favorites[next_id++] = cmd_str;
                }
                else if (WEXITSTATUS(status) != 0)
                {
                    std::cout << "El comando falló con el estado de salida " << WEXITSTATUS(status) << std::endl;
                }
            }
            else
            {
                std::cout << "El comando terminó de forma inesperada." << std::endl;
            }
        }
        else
        {
            perror("fork");
        }
    }
    else
    {
        // Comando 'favs' no se agrega a favoritos
        pid_t pid = fork();
        if (pid == 0)
        {
            // Proceso hijo
            auto cargs = convertToCharPointers(cmd_args);
            execvp(cargs[0], cargs.data());
            perror("execvp");
            exit(EXIT_FAILURE);
        }
        else if (pid > 0)
        {
            // Proceso padre
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
            {
                std::cout << "El comando 'favs' falló con el estado de salida " << WEXITSTATUS(status) << std::endl;
            }
        }
        else
        {
            perror("fork");
        }
    }
}

void executeWithPipes(const std::vector<std::vector<std::string> > &commands)
{
    int num_pipes = commands.size() - 1;
    std::vector<int> pipe_fds(num_pipes * 2);

    // Crear los pipes
    for (int i = 0; i < num_pipes; ++i)
    {
        if (pipe(pipe_fds.data() + i * 2) < 0)
        {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (size_t i = 0; i < commands.size(); ++i)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            // Proceso hijo
            if (i > 0)
            {
                dup2(pipe_fds[(i - 1) * 2], STDIN_FILENO);
            }
            if (i < num_pipes)
            {
                dup2(pipe_fds[i * 2 + 1], STDOUT_FILENO);
            }
            for (int fd : pipe_fds)
            {
                close(fd);
            }
            auto cargs = convertToCharPointers(commands[i]);
            execvp(cargs[0], cargs.data());
            perror("execvp");
            exit(EXIT_FAILURE);
        }
        else if (pid < 0)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
    }

    // Cerrar todos los descriptores de archivo en el proceso padre
    for (int fd : pipe_fds)
    {
        close(fd);
    }

    // Esperar a que todos los procesos hijos terminen
    for (size_t i = 0; i < commands.size(); ++i)
    {
        int status;
        wait(&status);
    }
}

void loadFavorites()
{
    if (favorites_file.empty())
        return;
    std::ifstream ifs(favorites_file);
    std::string line;
    while (std::getline(ifs, line))
    {
        std::istringstream iss(line);
        int id;
        std::string cmd;
        if (iss >> id)
        {
            std::getline(iss, cmd);
            favorites[id] = cmd.substr(1); // Eliminar espacio inicial
            next_id = std::max(next_id, id + 1);
        }
    }
}

void executeFavsCommand(const std::vector<std::string> &args)
{
    if (args.empty() || args[0] == "--help")
    {
        std::cout << "Uso de 'favs':" << std::endl;
        std::cout << "  --help           Muestra esta ayuda" << std::endl;
        std::cout << "  --crear <ruta>   Crea archivo para almacenar favoritos en <ruta>" << std::endl;
        std::cout << "  --mostrar        Muestra la lista de comandos favoritos" << std::endl;
        std::cout << "  --eliminar <nums> Elimina comandos con los números proporcionados, separados por comas" << std::endl;
        std::cout << "  --buscar <cmd>   Busca comandos que contengan la subcadena <cmd>" << std::endl;
        std::cout << "  --borrar         Borra todos los comandos favoritos" << std::endl;
        std::cout << "  --ejecutar <num> Ejecuta el comando con el número proporcionado" << std::endl;
        std::cout << "  --cargar         Carga comandos desde el archivo de favoritos y muestra en pantalla" << std::endl;
        std::cout << "  --guardar        Guarda los comandos favoritos actuales en el archivo" << std::endl;
        return;
    }

    if (args[0] == "--crear")
    {
        if (args.size() < 2)
        {
            std::cout << "Uso: favs --crear <ruta/misfavoritos.txt>" << std::endl;
            return;
        }
        favorites_file = args[1];
        std::cout << "Archivo de favoritos creado en " << favorites_file << std::endl;
    }
    else if (args[0] == "--mostrar")
    {
        if (favorites.empty())
        {
            std::cout << "No hay comandos favoritos." << std::endl;
        }
        else
        {
            for (const auto &[id, cmd] : favorites)
            {
                std::cout << id << ": " << cmd << std::endl;
            }
        }
    }
    else if (args[0] == "--eliminar")
    {
        if (args.size() < 2)
        {
            std::cout << "Uso: favs --eliminar num1,num2,..." << std::endl;
            return;
        }
        std::istringstream iss(args[1]);
        std::string num;
        while (std::getline(iss, num, ','))
        {
            int id = std::stoi(num);
            favorites.erase(id);
        }
    }
    else if (args[0] == "--buscar")
    {
        if (args.size() < 2)
        {
            std::cout << "Uso: favs --buscar <cmd>" << std::endl;
            return;
        }
        std::string search_str = args[1];
        bool found = false;
        for (const auto &[id, cmd] : favorites)
        {
            if (cmd.find(search_str) != std::string::npos)
            {
                std::cout << id << ": " << cmd << std::endl;
                found = true;
            }
        }
        if (!found)
        {
            std::cout << "No se encontraron comandos que coincidan." << std::endl;
        }
    }
    else if (args[0] == "--borrar")
    {
        favorites.clear();
        std::cout << "Todos los comandos favoritos han sido borrados." << std::endl;
    }
    else if (args[0] == "--ejecutar")
    {
        if (args.size() < 2)
        {
            std::cout << "Uso: favs --ejecutar <num>" << std::endl;
            return;
        }
        int id = std::stoi(args[1]);
        if (favorites.find(id) != favorites.end())
        {
            std::vector<std::string> cmd_args = parseInput(favorites[id]);
            executeCommand(cmd_args);
        }
        else
        {
            std::cout << "Comando con número " << id << " no encontrado." << std::endl;
        }
    }
    else if (args[0] == "--cargar")
    {
        if (favorites_file.empty())
        {
            std::cout << "Primero debes crear un archivo de favoritos." << std::endl;
            return;
        }
        loadFavorites();
        std::cout << "Favoritos cargados desde " << favorites_file << std::endl;
    }
    else if (args[0] == "--guardar")
    {
        saveFavorites();
        std::cout << "Favoritos guardados en " << favorites_file << std::endl;
    }
    else
    {
        std::cout << "Opción no válida. Usa '--help' para más información." << std::endl;
    }
}

void setReminder(int seconds, const std::string &message)
{
    std::thread([seconds, message]()
                {
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
        std::cout << "Recordatorio: " << message << std::endl << "Presione Enter para salir..." << std::endl; })
        .detach();
    std::cout << "Recordatorio establecido en " << seconds << " segundos." << std::endl;
}

// Función principal
int main()
{
    std::string input;
    while (true)
    {
        std::cout << "shell> ";
        std::getline(std::cin, input);
        auto args = parseInput(input);
        if (args.empty())
            continue;
        if (args[0] == "exit")
            break;
        if (args[0] == "favs")
        {
            args.erase(args.begin());
            executeFavsCommand(args);
        }
        else if (args[0] == "set" && args.size() > 2 && args[1] == "recordatorio")
        {
            int seconds = std::stoi(args[2]);
            std::string message;
            for (size_t i = 3; i < args.size(); ++i)
            {
                message += args[i] + " ";
            }
            setReminder(seconds, message);
        }
        else
        {
            std::vector<std::string> cmd_args = parseInput(input);
            executeCommand(cmd_args);
        }
    }
    return 0;
}