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

map<int, string> favorites;
int next_id = 1;
string favorites_file = "";

void saveFavorites()
{
    if (favorites_file.empty())
        return;
    ofstream ofs(favorites_file);
    for (const auto &[id, cmd] : favorites)
    {
        ofs << id << " " << cmd << endl;
    }
}

vector<char *> convertToCharPointers(const vector<string> &args)
{
    vector<char *> cargs;
    for (const auto &arg : args)
    {
        cargs.push_back(const_cast<char *>(arg.c_str()));
    }
    cargs.push_back(nullptr);
    return cargs;
}

vector<string> parseInput(const string &input)
{
    istringstream iss(input);
    vector<string> args;
    string arg;
    while (iss >> arg)
    {
        args.push_back(arg);
    }
    return args;
}

void executeCommand(const vector<string> &cmd_args)
{
    if (cmd_args.empty())
        return;

    string cmd_str = cmd_args[0];
    for (size_t i = 1; i < cmd_args.size(); ++i)
    {
        cmd_str += " " + cmd_args[i];
    }
    bool is_favs_command = (cmd_args[0] == "favs");

    if (!is_favs_command)
    {
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
            auto cargs = convertToCharPointers(cmd_args);
            execvp(cargs[0], cargs.data());
            perror("execvp");
            exit(EXIT_FAILURE);
        }
        else if (pid > 0)
        {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status))
            {
                if (WEXITSTATUS(status) == 0 && !command_exists)
                {
                    favorites[next_id++] = cmd_str;
                }
                else if (WEXITSTATUS(status) != 0)
                {
                    cout << "El comando falló con el estado de salida " << WEXITSTATUS(status) << endl;
                }
            }
            else
            {
                cout << "El comando terminó de forma inesperada." << endl;
            }
        }
        else
        {
            perror("fork");
        }
    }
    else
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            auto cargs = convertToCharPointers(cmd_args);
            execvp(cargs[0], cargs.data());
            perror("execvp");
            exit(EXIT_FAILURE);
        }
        else if (pid > 0)
        {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
            {
                cout << "El comando 'favs' falló con el estado de salida " << WEXITSTATUS(status) << endl;
            }
        }
        else
        {
            perror("fork");
        }
    }
}

void executeWithPipes(const string &full_command, const vector<vector<string>> &commands)
{
    int num_pipes = commands.size() - 1;
    int pipefds[2 * num_pipes];
    for (int i = 0; i < num_pipes; i++)
    {
        if (pipe(pipefds + i * 2) == -1)
        {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }
    int pid;
    int j = 0;
    for (int i = 0; i < commands.size(); i++)
    {
        pid = fork();
        if (pid == 0)
        {
            if (i > 0)
            {
                if (dup2(pipefds[j - 2], STDIN_FILENO) < 0)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            if (i < num_pipes)
            {
                if (dup2(pipefds[j + 1], STDOUT_FILENO) < 0)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            for (int k = 0; k < 2 * num_pipes; k++)
            {
                close(pipefds[k]);
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
        j += 2;
    }
    for (int i = 0; i < 2 * num_pipes; i++)
    {
        close(pipefds[i]);
    }
    for (int i = 0; i < commands.size(); i++)
    {
        wait(NULL);
    }
    if (full_command.substr(0, 4) != "favs")
    {
        favorites[next_id++] = full_command;
    }
}

void loadFavorites()
{
    if (favorites_file.empty())
        return;
    ifstream ifs(favorites_file);
    string line;
    while (getline(ifs, line))
    {
        istringstream iss(line);
        int id;
        string cmd;
        if (iss >> id)
        {
            getline(iss, cmd);
            favorites[id] = cmd.substr(1);
            next_id = max(next_id, id + 1);
        }
    }
}

void executeFavsCommand(const vector<string> &args)
{
    if (args.empty() || args[0] == "--help")
    {
        cout << "Uso de 'favs':" << endl;
        cout << "  --help           Muestra esta ayuda" << endl;
        cout << "  --crear <ruta>   Crea archivo para almacenar favoritos en <ruta>" << endl;
        cout << "  --mostrar        Muestra la lista de comandos favoritos" << endl;
        cout << "  --eliminar <nums> Elimina comandos con los números proporcionados, separados por comas" << endl;
        cout << "  --buscar <cmd>   Busca comandos que contengan la subcadena <cmd>" << endl;
        cout << "  --borrar         Borra todos los comandos favoritos" << endl;
        cout << "  --ejecutar <num> Ejecuta el comando con el número proporcionado" << endl;
        cout << "  --cargar         Carga comandos desde el archivo de favoritos y muestra en pantalla" << endl;
        cout << "  --guardar        Guarda los comandos favoritos actuales en el archivo" << endl;
        return;
    }
    if (args[0] == "--crear")
    {
        if (args.size() < 2)
        {
            cout << "Uso: favs --crear <ruta/misfavoritos.txt>" << endl;
            return;
        }
        favorites_file = args[1];
        cout << "Archivo de favoritos creado en " << favorites_file << endl;
        saveFavorites();
    }
    else if (args[0] == "--mostrar")
    {
        if (favorites.empty())
        {
            cout << "No hay comandos favoritos." << endl;
        }
        else
        {
            for (const auto &[id, cmd] : favorites)
            {
                cout << id << ": " << cmd << endl;
            }
        }
    }
    else if (args[0] == "--eliminar")
    {
        if (args.size() < 2)
        {
            cout << "Uso: favs --eliminar num1,num2,..." << endl;
            return;
        }
        istringstream iss(args[1]);
        string num;
        while (getline(iss, num, ','))
        {
            int id = stoi(num);
            favorites.erase(id);
        }
    }
    else if (args[0] == "--buscar")
    {
        if (args.size() < 2)
        {
            cout << "Uso: favs --buscar <cmd>" << endl;
            return;
        }
        string search_str = args[1];
        bool found = false;
        for (const auto &[id, cmd] : favorites)
        {
            if (cmd.find(search_str) != string::npos)
            {
                cout << id << ": " << cmd << endl;
                found = true;
            }
        }
        if (!found)
        {
            cout << "No se encontraron comandos que coincidan." << endl;
        }
    }
    else if (args[0] == "--borrar")
    {
        favorites.clear();
        next_id = 1;
        cout << "Todos los comandos favoritos han sido borrados." << endl;
    }
    else if (args[0] == "--ejecutar")
    {
        if (args.size() < 2)
        {
            cout << "Uso: favs --ejecutar <num>" << endl;
            return;
        }
        int id = stoi(args[1]);
        if (favorites.find(id) != favorites.end())
        {
            vector<string> cmd_args = parseInput(favorites[id]);
            executeCommand(cmd_args);
        }
        else
        {
            cout << "Comando con número " << id << " no encontrado." << endl;
        }
    }
    else if (args[0] == "--cargar")
    {
        if (favorites_file.empty())
        {
            cout << "Primero debes crear un archivo de favoritos." << endl;
            return;
        }
        loadFavorites();
        cout << "Favoritos cargados desde " << favorites_file << endl;
    }
    else if (args[0] == "--guardar")
    {
        saveFavorites();
        cout << "Favoritos guardados en " << favorites_file << endl;
    }
    else
    {
        cout << "Opción no válida. Usa '--help' para más información." << endl;
    }
}

void setReminder(int seconds, const string &message)
{
    thread([seconds, message]()
           {
        this_thread::sleep_for(chrono::seconds(seconds));
        cout << "Recordatorio: " << message << endl << "Presione Enter para salir..." << endl; })
        .detach();
    cout << "Recordatorio establecido en " << seconds << " segundos." << endl;
}

int main()
{
    string input;
    while (true)
    {
        cout << "SkibidiShell:$ ";
        getline(cin, input);
        string input_copy = input;
        vector<string> pipe_segments;
        string delimiter = "|";
        size_t pos = 0;
        while ((pos = input.find(delimiter)) != string::npos)
        {
            pipe_segments.push_back(input.substr(0, pos));
            input.erase(0, pos + delimiter.length());
        }
        pipe_segments.push_back(input);

        if (pipe_segments.size() == 1)
        {
            auto args = parseInput(pipe_segments[0]);
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
                int seconds = stoi(args[2]);
                string message;
                for (size_t i = 3; i < args.size(); ++i)
                {
                    message += args[i] + " ";
                }
                setReminder(seconds, message);
            }
            else
            {
                vector<string> cmd_args = parseInput(input);
                executeCommand(cmd_args);
            }
        }
        else
        {
            vector<vector<string>> commands_with_args;
            for (const auto &segment : pipe_segments)
            {
                commands_with_args.push_back(parseInput(segment));
            }
            executeWithPipes(input_copy, commands_with_args);
        }
    }
    return 0;
}
