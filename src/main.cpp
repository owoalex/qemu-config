/*

qemu-config v1.0.0
Copyright (c) Alex Baldwin 2019.

qemu-config is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License,
version 2 exclusively, as published by the Free Software Foundation.

qemu-config is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with qemu-config. If not, see:
https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt/

*/

#include <iostream>

#include <gtk/gtk.h>
#include "json.hpp"
#include <stdlib.h>
#include <cstring>

#include <stdio.h>
#include <unistd.h>

#include <sys/stat.h> // stat
#include <errno.h>    // errno, ENOENT, EEXIST
#if defined(_WIN32)
#include <direct.h>   // _mkdir
#endif

using json = nlohmann::json;

std::string qemuConfigFolder = std::string(getenv("HOME")) + "/.qemu-config";
std::string vmList = "";
std::string vmLogFile = "";
std::string vmPidFile = "";

bool isDirExist(const std::string& path)
{
#if defined(_WIN32)
    struct _stat info;
    if (_stat(path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & _S_IFDIR) != 0;
#else 
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
#endif
}

bool makePath(const std::string& path)
{
#if defined(_WIN32)
    int ret = _mkdir(path.c_str());
#else
    mode_t mode = 0755;
    int ret = mkdir(path.c_str(), mode);
#endif
    if (ret == 0)
        return true;

    switch (errno)
    {
    case ENOENT:
        // parent didn't exist, try to create it
        {
            int pos = path.find_last_of('/');
            if (pos == std::string::npos)
#if defined(_WIN32)
                pos = path.find_last_of('\\');
            if (pos == std::string::npos)
#endif
                return false;
            if (!makePath( path.substr(0, pos) ))
                return false;
        }
        // now, try to create again
#if defined(_WIN32)
        return 0 == _mkdir(path.c_str());
#else 
        return 0 == mkdir(path.c_str(), mode);
#endif

    case EEXIST:
        // done!
        return isDirExist(path);

    default:
        return false;
    }
}

static void launchQEMU (GtkWidget *widget, gpointer userData){
    
    std::string command = "qemu/qemu-system-x86_64 ";
    
    command += "-L ./qemu ";
    command += "--bios ./bios/efi/OVMF.fd ";
    command += "-cdrom archlinux-2019.11.01-x86_64.iso ";
    command += "-vga virtio ";
    command += "-m 2048 ";
    command += "-cpu host,hv_relaxed,hv_spinlocks=0x1fff,hv_vapic,hv_time ";
    command += "-enable-kvm ";
    command += "-smp cores=2 ";
    command += "-drive file=virtHDD.img,format=raw,index=0,media=disk,cache.direct=on ";
    command += "-soundhw ac97 ";
    command += "-netdev user,id=n0 -device rtl8139,netdev=n0 ";
    command += "-pidfile " + vmPidFile + " ";
    command += "-D " + vmLogFile;
    command += " &>> " + vmLogFile;
    
    std::cout << "\nExecuting: \n" << command << "\n\n";
    
    std::system(command.c_str());
}

static void activate (GtkApplication* app,gpointer userData) {
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *buttonBox;

    window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (window), "qemu-config");
    gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);
    gtk_widget_show_all (window);
    
    buttonBox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add (GTK_CONTAINER (window), buttonBox);

    button = gtk_button_new_with_label ("Boot VM");
    g_signal_connect (button, "clicked", G_CALLBACK (launchQEMU), NULL);
    //g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_destroy), window);
    gtk_container_add (GTK_CONTAINER (buttonBox), button);

    gtk_widget_show_all (window);
}


int main(int argc, char** argv) {
    std::cout << "qemu-config v1.0.0\nCopyright (c) Alex Baldwin 2019.\n\n";

    std::string bootVmImmediate = "";

	for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-f") {
            i++;
            std::string arg2 = argv[i];
            qemuConfigFolder = arg2;
            continue;
        }
        bootVmImmediate = arg;
    }
    
    vmList = qemuConfigFolder + "/vms.json";
    vmLogFile = qemuConfigFolder + "/vm.log";
    vmPidFile = qemuConfigFolder + "/vm.pid";
    
    std::cout << "qemu-config folder       :" << qemuConfigFolder << "\n";
    std::cout << "Config file              :" << vmList << "\n";
    std::cout << "VM Log file              :" << vmLogFile << "\n";
    std::cout << "VM to autoboot           :" << bootVmImmediate << "\n\n";

    makePath(qemuConfigFolder);
    
    GtkApplication *app;
    int status;

    app = gtk_application_new ("com.indentationerror.qemu-config", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
    status = g_application_run (G_APPLICATION (app),0,NULL);
    g_object_unref (app);

    return status;
}
