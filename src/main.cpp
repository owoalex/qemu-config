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
    command += "-cdrom cdrom.iso ";
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

static void killQEMU (GtkWidget *widget, gpointer userData){
    
    std::string command = "kill $(cat " + vmPidFile + ")";
    
    std::cout << "\nExecuting: \n" << command << "\n\n";
    
    std::system(command.c_str());
}

// GTK_ICON_SIZE_MENU
// 	
// 
// Size appropriate for menus (16px).
// 	 
// 
// GTK_ICON_SIZE_SMALL_TOOLBAR
// 	
// 
// Size appropriate for small toolbars (16px).
// 	 
// 
// GTK_ICON_SIZE_LARGE_TOOLBAR
// 	
// 
// Size appropriate for large toolbars (24px)
// 	 
// 
// GTK_ICON_SIZE_BUTTON
// 	
// 
// Size appropriate for buttons (16px)
// 	 
// 
// GTK_ICON_SIZE_DND
// 	
// 
// Size appropriate for drag and drop (32px)
// 	 
// 
// GTK_ICON_SIZE_DIALOG
// 	
// 
// Size appropriate for dialogs (48px)


static void activate (GtkApplication* app,gpointer userData) {
    
    GtkWidget *window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (window), "qemu-config");
    gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
    gtk_widget_show_all (window);

    GtkWidget *mainPane = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add(GTK_CONTAINER(window), mainPane);
    //gtk_grid_attach(GTK_GRID(mainGrid), buttonBox, 1, 0, 1, 1);
    GtkWidget *leftBox = gtk_box_new (GTK_ORIENTATION_VERTICAL,8);
    GtkWidget *rightBox = gtk_box_new (GTK_ORIENTATION_VERTICAL,8);
    
    
    
    GtkWidget *vmListManagerBar = gtk_action_bar_new();
    gtk_box_pack_start(GTK_BOX(leftBox),vmListManagerBar,false,true,0);
    GtkWidget *buttonNewVM = gtk_button_new_from_icon_name("document-new", GTK_ICON_SIZE_BUTTON);
    //g_signal_connect (buttonBootVM, "clicked", G_CALLBACK(launchQEMU), NULL);
    gtk_action_bar_pack_start(GTK_ACTION_BAR(vmListManagerBar),buttonNewVM);
    
    //GtkWidget *buttonDeleteVM = gtk_button_new_with_label("Delete");
    GtkWidget *buttonDeleteVM = gtk_button_new_from_icon_name("edit-delete", GTK_ICON_SIZE_BUTTON);
    //g_signal_connect (buttonBootVM, "clicked", G_CALLBACK(launchQEMU), NULL);
    gtk_action_bar_pack_start(GTK_ACTION_BAR(vmListManagerBar),buttonDeleteVM);
    
    GtkWidget *scrolledVmList = gtk_scrolled_window_new (NULL, NULL);
    GtkWidget *vmListContainer = gtk_box_new (GTK_ORIENTATION_VERTICAL,8);
    gtk_container_add (GTK_CONTAINER (scrolledVmList), vmListContainer);
    gtk_box_pack_start(GTK_BOX(leftBox),scrolledVmList,true,true,0);
    
    
    GtkWidget *buttonSelectVM = gtk_button_new_with_label("VM-Name");
    g_signal_connect (buttonSelectVM, "clicked", G_CALLBACK(killQEMU), NULL);
    gtk_box_pack_start(GTK_BOX(vmListContainer),buttonSelectVM,false,true,0);
    
    
    gtk_paned_add1(GTK_PANED(mainPane), leftBox);
    
    
    
    GtkWidget *vmActionBar = gtk_action_bar_new();
    
    GtkWidget *buttonBootVM = gtk_button_new_with_mnemonic("_Boot");
    g_signal_connect (buttonBootVM, "clicked", G_CALLBACK(launchQEMU), NULL);
    gtk_action_bar_pack_end(GTK_ACTION_BAR(vmActionBar),buttonBootVM);
    
    GtkWidget *buttonKillVM = gtk_button_new_with_mnemonic("_Kill");
    g_signal_connect (buttonKillVM, "clicked", G_CALLBACK(killQEMU), NULL);
    gtk_action_bar_pack_end(GTK_ACTION_BAR(vmActionBar),buttonKillVM);
    
    GtkWidget *scrolledSettings = gtk_scrolled_window_new (NULL, NULL);
    GtkWidget *settingsContainer = gtk_grid_new();
    gtk_container_add (GTK_CONTAINER(scrolledSettings), settingsContainer);
    gtk_box_pack_start(GTK_BOX(rightBox),scrolledSettings,true,true,0);
    
    GtkWidget *settingsLabel1 = gtk_label_new("VM Name");
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsLabel1, 1, 1, 1, 1);
    GtkWidget *settingsInput1 = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsInput1, 2, 1, 1, 1);
    
    GtkWidget *settingsLabel2 = gtk_label_new("CPU Cores");
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsLabel2, 1, 2, 1, 1);
    
    gtk_box_pack_end(GTK_BOX(rightBox),vmActionBar,false,true,0);
    gtk_paned_add2(GTK_PANED(mainPane), rightBox);
    
    //g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_destroy), window);
    

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
