# Download and install on Linux

**[Download FreeShip 26 for Linux (x86_64)](https://github.com/jiangshirong/FreeShip-26/releases/latest/download/FreeShip-26-Linux-x86_64.flatpak)**

After downloading, double-click the `.flatpak` file and follow the installation prompts.

### If you see the “Open With” window

If double-clicking the file opens the **Open With** window shown below, click **Cancel**.

![Open With dialog showing no recommended application for the FreeShip Flatpak bundle](assets/linux-install/open-with.png)

- For **Ubuntu or Debian**, run `sudo apt install flatpak gnome-software gnome-software-plugin-flatpak`.
- For **Fedora Workstation (GNOME)**, run `sudo dnf install flatpak gnome-software`.
- For **Arch Linux**, run `sudo pacman -S --needed flatpak gnome-software`.
- For **other Linux distributions**, ask an AI assistant how to install a graphical Flatpak installer for your distribution.

Double-click the downloaded `.flatpak` file again. If **Open With** still appears, select the software center you installed and set it as the default for this file type. Click **Install**. When installation finishes, open the applications menu and search for **FreeShip 26**. If you just installed Flatpak and the app does not appear, sign out of your desktop session and sign back in.
