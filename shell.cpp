#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <sstream>
#include <fstream>
#include <map>
#include <csignal>
#include <thread>
#include <chrono>

using namespace std;

map<int, string> favoritos;
int favCounter = 1;
string favFilePath = "";

void handleSigint(int sig)
{
    cout << "\nSalida con Ctrl+C detectada. Guardando favoritos..." << endl;
    // Llamar a la función de guardado de favoritos
    if (!favFilePath.empty())
    {
        ofstream ofs(favFilePath);
        if (ofs)
        {
            for (const auto &pair : favoritos)
            {
                ofs << pair.second << endl;
            }
            cout << "Comandos favoritos guardados en: " << favFilePath << endl;
        }
        else
        {
            cerr << "Error al guardar el archivo de favoritos." << endl;
        }
    }
    exit(0);
}

vector<string> splitArgs(const string &command)
{
    istringstream stream(command);
    string arg;
    vector<string> args;
    while (stream >> arg)
    {
        args.push_back(arg);
    }
    return args;
}

void executeCommand(const vector<string> &args)
{
    vector<char *> c_args;
    for (const string &arg : args)
    {
        c_args.push_back(const_cast<char *>(arg.c_str()));
    }
    c_args.push_back(nullptr);

    execvp(c_args[0], c_args.data());
    perror("Command execution failed");
    exit(EXIT_FAILURE);
}

void executePipe(const vector<string> &commands)
{
    int pipefd[2];
    pid_t pid;
    int fd_in = 0;

    for (size_t i = 0; i < commands.size(); ++i)
    {
        pipe(pipefd);
        pid = fork();

        if (pid == -1)
        {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            dup2(fd_in, 0);
            if (i < commands.size() - 1)
            {
                dup2(pipefd[1], 1);
            }
            close(pipefd[0]);

            vector<string> args = splitArgs(commands[i]);
            executeCommand(args);
        }
        else
        {
            wait(NULL);
            close(pipefd[1]);
            fd_in = pipefd[0];
        }
    }
}

void favsCrear(const string &path)
{
    favFilePath = path;
    ofstream ofs(favFilePath);
    if (ofs)
    {
        cout << "Archivo de favoritos creado en: " << path << endl;
    }
    else
    {
        cerr << "Error al crear el archivo de favoritos" << endl;
    }
}

void favsMostrar()
{
    for (const auto &pair : favoritos)
    {
        cout << pair.first << ": " << pair.second << endl;
    }
}

void favsEliminar(const vector<int> &nums)
{
    for (int num : nums)
    {
        favoritos.erase(num);
    }
}

void favsBuscar(const string &cmd)
{
    for (const auto &pair : favoritos)
    {
        if (pair.second.find(cmd) != string::npos)
        {
            cout << pair.first << ": " << pair.second << endl;
        }
    }
}

void favsBorrar()
{
    favoritos.clear();
    cout << "Todos los comandos favoritos han sido borrados." << endl;
}

void favsEjecutar(int num)
{
    if (favoritos.find(num) != favoritos.end())
    {
        vector<string> args = splitArgs(favoritos[num]);
        if (fork() == 0)
        {
            executeCommand(args);
        }
        else
        {
            wait(NULL);
        }
    }
    else
    {
        cerr << "Comando no encontrado en favoritos." << endl;
    }
}

void favsCargar()
{
    if (favFilePath.empty())
    {
        cerr << "Ruta del archivo de favoritos no establecida." << endl;
        return;
    }
    ifstream ifs(favFilePath);
    if (ifs)
    {
        string line;
        while (getline(ifs, line))
        {
            favoritos[favCounter++] = line;
        }
        favsMostrar();
    }
    else
    {
        cerr << "Error al leer el archivo de favoritos." << endl;
    }
}

void favsGuardar()
{
    if (favFilePath.empty())
    {
        cerr << "Ruta del archivo de favoritos no establecida." << endl;
        return;
    }
    ofstream ofs(favFilePath);
    if (ofs)
    {
        for (const auto &pair : favoritos)
        {
            ofs << pair.second << endl;
        }
        cout << "Comandos favoritos guardados en: " << favFilePath << endl;
    }
    else
    {
        cerr << "Error al guardar el archivo de favoritos." << endl;
    }
}

void favsHelp()
{
    cout << "Uso del comando favs:\n"
         << "  --crear <ruta/archivo>: Crea un archivo para almacenar los comandos favoritos.\n"
         << "  --mostrar: Muestra todos los comandos favoritos con sus números asociados.\n"
         << "  --eliminar <num1,num2,...>: Elimina los comandos favoritos especificados por sus números.\n"
         << "  --buscar <cmd>: Busca comandos que contengan 'cmd' en la lista de favoritos.\n"
         << "  --borrar: Borra todos los comandos favoritos.\n"
         << "  --ejecutar <num>: Ejecuta el comando favorito especificado por su número.\n"
         << "  --cargar: Carga comandos favoritos desde un archivo.\n"
         << "  --guardar: Guarda los comandos favoritos en un archivo.\n"
         << "  --help: Muestra esta ayuda.\n";
}

void handleFavsCommand(const vector<string> &args)
{
    if (args.size() < 2)
    {
        cerr << "Uso incorrecto del comando favs. Use --help para más información." << endl;
        return;
    }

    if (args[1] == "--crear" && args.size() == 3)
    {
        favsCrear(args[2]);
    }
    else if (args[1] == "--mostrar")
    {
        favsMostrar();
    }
    else if (args[1] == "--eliminar" && args.size() == 3)
    {
        vector<string> nums_str = splitArgs(args[2]);
        vector<int> nums;
        for (const string &num_str : nums_str)
        {
            nums.push_back(stoi(num_str));
        }
        favsEliminar(nums);
    }
    else if (args[1] == "--buscar" && args.size() == 3)
    {
        favsBuscar(args[2]);
    }
    else if (args[1] == "--borrar")
    {
        favsBorrar();
    }
    else if (args[1] == "--cargar")
    {
        favsCargar();
    }
    else if (args[1] == "--guardar")
    {
        favsGuardar();
    }
    else if (args[1] == "--ejecutar" && args.size() == 3)
    {
        favsEjecutar(stoi(args[2]));
    }
    else if (args[1] == "--help")
    {
        favsHelp();
    }
    else
    {
        cerr << "Comando favs no válido. Use --help para más información." << endl;
    }
}

void setReminder(int seconds, const string &message)
{
    this_thread::sleep_for(chrono::seconds(seconds));
    cout << "\nRecordatorio: " << message << "\nPresione Enter para salir..." << endl;
}

void handleCustomCommand(const vector<string> &args)
{
    if (args.size() >= 3 && args[0] == "set" && args[1] == "recordatorio")
    {
        try
        {
            int seconds = stoi(args[2]);
            string message;
            if (args.size() > 3)
            {
                for (size_t i = 3; i < args.size(); ++i)
                {
                    message += args[i] + " ";
                }
            }
            message = message.substr(0, message.size() - 1); // Eliminar el último espacio
            thread reminderThread(setReminder, seconds, message);
            reminderThread.detach(); // Desacoplar el hilo para que funcione en segundo plano
            cout << "Recordatorio establecido para " << seconds << " segundos.\n";
        }
        catch (const invalid_argument &)
        {
            cerr << "Error: El segundo argumento debe ser un número entero (segundos)." << endl;
        }
    }
    else
    {
        cerr << "Comando desconocido o mal formateado. Uso: set recordatorio <segundos> \"<mensaje>\"" << endl;
    }
}

int main()
{
    signal(SIGINT, handleSigint); // Manejar Ctrl+C
    string input;
    vector<string> commands;
    while (true)
    {
        cout << "mishell:$ ";
        getline(cin, input);

        if (input.empty())
        {
            continue;
        }

        if (input == "exit")
        {
            handleSigint(SIGINT); // Llamar al handler de salida para simular el exit
            break;
        }

        vector<string> args = splitArgs(input);

        if (args[0] == "favs")
        {
            handleFavsCommand(args);
        }
        else if (args[0] == "set" && args[1] == "recordatorio")
        {
            handleCustomCommand(args);
        }
        else
        {
            if (favoritos.empty() || favoritos.find(favCounter) == favoritos.end())
            {
                favoritos[favCounter++] = input;
            }
            commands.push_back(input);
            executePipe(commands);
            commands.clear();
        }
    }
    return 0;
}
