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
#include <fstream>
#include <streambuf>

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
json vmOptions;

class Drive {
public:
    std::string virtualType;
    std::string realType;
    std::string file;
    
    Drive(json options) {
        std::cout << "New Drive" << "\n";
        std::cout << "    Real Type:          " << options["realType"].get<std::string>() << "\n";
        std::cout << "    Emulated Type:      " << options["virtualType"].get<std::string>() << "\n";
        std::cout << "    File:               " << options["file"].get<std::string>() << "\n";
        virtualType = options["virtualType"].get<std::string>();
        realType = options["realType"].get<std::string>();
        file = options["file"].get<std::string>();
    }
};

class VMObj {
public:
    std::string name;
    bool efi;
    int memory;
    int cores;
    std::string graphics;
    std::string sound;
    std::string network;
    std::vector<Drive> drives;
    
    VMObj(json options) {
        std::cout << "New VM" << "\n";
        std::cout << "Name:               " << options["name"].get<std::string>() << "\n";
        std::cout << "EFI?:               " << options["efi"].get<bool>() << "\n";
        std::cout << "Memory:             " << options["memory"].get<int>() << "\n";
        std::cout << "Cores:              " << options["cores"].get<int>() << "\n";
        std::cout << "Graphics Card:      " << options["graphics"]["driver"].get<std::string>() << "\n";
        std::cout << "Sound Card:         " << options["sound"].get<std::string>() << "\n";
        std::cout << "Network Card:       " << options["network"].get<std::string>() << "\n";
        std::cout << "[ Drives ]             " << "\n";
        
        for (auto& element : options["drives"]) {
            Drive drive(element);
            drives.push_back(drive);
        }
        
        name = options["name"].get<std::string>();
        efi = options["efi"].get<bool>();
        memory = options["memory"].get<int>();
        cores = options["cores"].get<int>();
        graphics = options["graphics"]["driver"].get<std::string>();
        sound = options["sound"].get<std::string>();
        network = options["network"].get<std::string>();
        
        std::cout << "" << "\n";
    }
};

std::vector<VMObj> vms;
VMObj *selectedVM;

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

GtkWidget *settingsInputName;
GtkWidget *settingsInputMemory;
GtkWidget *settingsInputCores;
GtkWidget *settingsInputEFI;
GtkWidget *settingsInputGraphicsDriver;
GtkWidget *settingsInputNetworkCard;
GtkWidget *settingsInputSoundCard;

static void launchQEMU (GtkWidget *widget, gpointer userData){
    std::cout << "Launching vm " << selectedVM->name << "\n";
    
    std::string command = "nohup qemu/qemu-system-x86_64 ";
    command += "-L ./qemu ";
    
    if (selectedVM->efi) {
        command += "--bios ./bios/efi/OVMF.fd ";
    } else {
        command += "--bios ./bios/bios-256k.bin ";
    }
    //command += "-cdrom Win7_Pro_SP1_English_x64.iso ";
    command += "-vga " + selectedVM->graphics + " ";
    command += "-m " + std::to_string(selectedVM->memory) + " ";
    
    //command += "-cpu qemu64,hv_relaxed,hv_spinlocks=0x1fff,hv_vapic,hv_time ";
    
    command += "-cpu host,hv_relaxed,hv_spinlocks=0x1fff,hv_vapic,hv_time ";
    command += "-enable-kvm ";
    
    command += "-smp cores=" + std::to_string(selectedVM->cores) + " ";
    int driveidx = 0;
    for (Drive& drive : selectedVM->drives) {
        std::cout << "Adding drive " << drive.file << "\n";
        command += "-drive file=" + drive.file + ",format=" + drive.realType + ",media=" + drive.virtualType + ",cache.direct=on,bus=" + std::to_string(driveidx) + ",unit=0 ";
        driveidx++;
    }
    command += "-soundhw " + selectedVM->sound + " ";
    command += "-netdev user,id=n0 -device " + selectedVM->network + ",netdev=n0 ";
    command += "-pidfile " + vmPidFile + " ";
    command += "-D " + vmLogFile;
    command += " & >> " + vmLogFile + "";

    std::cout << "\nExecuting: \n" << command << "\n\n";
    
    std::system(command.c_str());
}

static void killQEMU (GtkWidget *widget, gpointer userData){
    
    std::string command = "kill $(cat " + vmPidFile + ")";
    
    std::cout << "\nExecuting: \n" << command << "\n\n";
    
    std::system(command.c_str());
}

static void selectVM (GtkWidget *widget, VMObj *obj){
    std::cout << "Selecting vm ";
    //VMObj *obj = userData;
    selectedVM = obj;
    std::cout << selectedVM->name << "\n";
    gtk_entry_set_text(GTK_ENTRY(settingsInputName),selectedVM->name.c_str());
    gtk_entry_set_text(GTK_ENTRY(settingsInputCores),std::to_string(selectedVM->cores).c_str());
    gtk_entry_set_text(GTK_ENTRY(settingsInputMemory),std::to_string(selectedVM->memory).c_str());
    gtk_entry_set_text(GTK_ENTRY(settingsInputGraphicsDriver),selectedVM->graphics.c_str());
    gtk_entry_set_text(GTK_ENTRY(settingsInputSoundCard),selectedVM->sound.c_str());
    gtk_entry_set_text(GTK_ENTRY(settingsInputNetworkCard),selectedVM->network.c_str());
}

static void activate (GtkApplication* app,gpointer userData) {
    
    GtkWidget *window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (window), "qemu-config");
    gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
    gtk_widget_show_all (window);

    GtkWidget *mainPane = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add(GTK_CONTAINER(window), mainPane);
    //gtk_grid_attach(GTK_GRID(mainGrid), buttonBox, 1, 0, 1, 1);
    GtkWidget *leftBox = gtk_box_new (GTK_ORIENTATION_VERTICAL,0);
    GtkWidget *rightBox = gtk_box_new (GTK_ORIENTATION_VERTICAL,0);
    
    
    
    GtkWidget *vmListManagerBar = gtk_action_bar_new();
    gtk_box_pack_start(GTK_BOX(leftBox),vmListManagerBar,false,true,0);
    GtkWidget *buttonNewVM = gtk_button_new_from_icon_name("add", GTK_ICON_SIZE_BUTTON);
    //g_signal_connect (buttonBootVM, "clicked", G_CALLBACK(launchQEMU), NULL);
    gtk_action_bar_pack_start(GTK_ACTION_BAR(vmListManagerBar),buttonNewVM);
    
    //GtkWidget *buttonDeleteVM = gtk_button_new_with_label("Delete");
    GtkWidget *buttonDeleteVM = gtk_button_new_from_icon_name("edit-delete", GTK_ICON_SIZE_BUTTON);
    //g_signal_connect (buttonBootVM, "clicked", G_CALLBACK(launchQEMU), NULL);
    gtk_action_bar_pack_start(GTK_ACTION_BAR(vmListManagerBar),buttonDeleteVM);
    
    GtkWidget *scrolledVmList = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (reinterpret_cast<GtkScrolledWindow*>(scrolledVmList),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_ALWAYS);
    
    gtk_box_pack_start(GTK_BOX(leftBox),scrolledVmList,true,true,0); // add scrolledVm lisr to leftBox
    
    GtkWidget *vmListContainer = gtk_box_new(GTK_ORIENTATION_VERTICAL,8);
    gtk_container_add(GTK_CONTAINER (scrolledVmList), vmListContainer);
    
    for(VMObj& element: vms) {
        GtkWidget *buttonSelectVM = gtk_button_new_with_label(element.name.c_str());
        g_signal_connect (buttonSelectVM, "clicked", G_CALLBACK(selectVM), &element);
        gtk_box_pack_start(GTK_BOX(vmListContainer),buttonSelectVM,false,true,0);
        //std::cout << "Vm ptr ";
        //std::cout << &element << "\n";
    }
    
    gtk_paned_add1(GTK_PANED(mainPane), leftBox);
    
    
    
    GtkWidget *vmActionBar = gtk_action_bar_new();
    
    GtkWidget *buttonBootVM = gtk_button_new_with_mnemonic("_Boot");
    g_signal_connect (buttonBootVM, "clicked", G_CALLBACK(launchQEMU), NULL);
    gtk_action_bar_pack_end(GTK_ACTION_BAR(vmActionBar),buttonBootVM);
    
    GtkWidget *buttonKillVM = gtk_button_new_with_mnemonic("_Kill");
    g_signal_connect (buttonKillVM, "clicked", G_CALLBACK(killQEMU), NULL);
    gtk_action_bar_pack_end(GTK_ACTION_BAR(vmActionBar),buttonKillVM);
    
    GtkWidget *scrolledSettings = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (reinterpret_cast<GtkScrolledWindow*>(scrolledSettings),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_ALWAYS);
    GtkWidget *settingsContainer = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(settingsContainer),8);
    gtk_grid_set_column_spacing(GTK_GRID(settingsContainer),8);
    gtk_container_add(GTK_CONTAINER(scrolledSettings), settingsContainer);
    gtk_box_pack_start(GTK_BOX(rightBox),scrolledSettings,true,true,0);
    
    GtkWidget *settingsLabelName = gtk_label_new("VM Name");
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsLabelName, 1, 1, 1, 1);
    settingsInputName = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(settingsInputName),"null");
    gtk_widget_set_hexpand(settingsInputName, true);
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsInputName, 2, 1, 1, 1);
    
    GtkWidget *settingsLabelMemory = gtk_label_new("Memory");
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsLabelMemory, 1, 2, 1, 1);
    settingsInputMemory = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(settingsInputMemory),"2048");
    gtk_widget_set_hexpand(settingsInputMemory, true);
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsInputMemory, 2, 2, 1, 1);
    
    GtkWidget *settingsLabelCores = gtk_label_new("CPU Cores");
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsLabelCores, 1, 3, 1, 1);
    settingsInputCores = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(settingsInputCores),"1");
    gtk_widget_set_hexpand(settingsInputCores, true);
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsInputCores, 2, 3, 1, 1);
    
    GtkWidget *settingsLabelGraphicsDriver = gtk_label_new("Virtual Graphics Driver");
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsLabelGraphicsDriver, 1, 4, 1, 1);
    settingsInputGraphicsDriver = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(settingsInputGraphicsDriver),"virtio");
    gtk_widget_set_hexpand(settingsInputGraphicsDriver, true);
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsInputGraphicsDriver, 2, 4, 1, 1);
    
    GtkWidget *settingsLabelSoundCard = gtk_label_new("Virtual Sound Card");
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsLabelSoundCard, 1, 5, 1, 1);
    settingsInputSoundCard = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(settingsInputSoundCard),"ac97");
    gtk_widget_set_hexpand(settingsInputSoundCard, true);
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsInputSoundCard, 2, 5, 1, 1);
    
    GtkWidget *settingsLabelNetworkCard = gtk_label_new("Virtual Network Card");
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsLabelNetworkCard, 1, 6, 1, 1);
    settingsInputNetworkCard = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(settingsInputNetworkCard),"rtl8139");
    gtk_widget_set_hexpand(settingsInputNetworkCard, true);
    gtk_grid_attach(GTK_GRID(settingsContainer), settingsInputNetworkCard, 2, 6, 1, 1);
    
    
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
    
    std::ifstream t(vmList);
    std::string options;

    t.seekg(0, std::ios::end);
    options.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    options.assign((std::istreambuf_iterator<char>(t)),
                    std::istreambuf_iterator<char>());
    vmOptions = json::parse(options);
    
    for (auto& element : vmOptions) {
        //std::cout << element << '\n';
        VMObj vmobj(element);
        vms.push_back(vmobj);
    }
    
    GtkApplication *app;
    int status;

    app = gtk_application_new ("com.indentationerror.qemu-config", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
    status = g_application_run (G_APPLICATION (app),0,NULL);
    g_object_unref (app);

    return status;
}
