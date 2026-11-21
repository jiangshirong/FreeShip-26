# Download and install on Linux

**[Download FreeShip 26 for Linux (x86_64)](https://github.com/jiangshirong/FreeShip-26/releases/latest/download/FreeShip-26-Linux-x86_64.flatpak)**

After downloading, double-click the `.flatpak` file and follow the installation prompts. Tested on Ubuntu 24.04 and Debian 13.

### If you see the “Open With” window

If double-clicking the file opens the **Open With** window shown below, click **Cancel**.

![Open With dialog showing no recommended application for the FreeShip Flatpak bundle](assets/linux-install/open-with.png)

If Flatpak is already installed, open a terminal and run this command to install the latest version from GitHub:

```bash
flatpak install --user https://github.com/jiangshirong/FreeShip-26/releases/latest/download/FreeShip-26-Linux-x86_64.flatpak
```

If the terminal says `flatpak` was not found, [select your Linux distribution on the Flathub setup page](https://flathub.org/en/setup) and complete only the **Install Flatpak** step. Then run the command above again.
