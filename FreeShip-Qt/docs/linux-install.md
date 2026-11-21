# Download and install on Linux

**[Download FreeShip 26 for Linux (x86_64)](https://github.com/jiangshirong/FreeShip-26/releases/latest/download/FreeShip-26-Linux-x86_64.flatpak)**

After downloading, double-click the `.flatpak` file and follow the installation prompts. Tested on Ubuntu 24.04 and Debian 13.

### If you see the “Open With” window

If double-clicking the file opens the **Open With** window shown below, click **Cancel**.

![Open With dialog showing no recommended application for the FreeShip Flatpak bundle](assets/linux-install/open-with.png)

Double-click installation needs a software center that can open local Flatpak bundles. Choose the instructions for your distribution. If your account cannot install system packages, ask an administrator to do this step.

**Ubuntu 24.04 or Debian 13** — Install GNOME Software and its Flatpak plugin together:

```bash
sudo apt install flatpak gnome-software gnome-software-plugin-flatpak
```

**Fedora Workstation (GNOME)** — Try **Open With → Software** first. If Software is missing, install it with `sudo dnf install flatpak gnome-software`. [Fedora package details](https://packages.fedoraproject.org/pkgs/gnome-software/gnome-software/)

**Arch Linux** — To use GNOME Software, install both packages with `sudo pacman -S --needed flatpak gnome-software`. Arch lists Flatpak as an optional dependency for this feature. [Arch package details](https://archlinux.org/packages/extra/x86_64/gnome-software/)

**Other distributions** — Use your distribution’s package manager to install a graphical handler for local `.flatpak` files, such as GNOME Software with Flatpak support or KDE Discover with its Flatpak backend. Package names vary. The [Flathub setup directory](https://flathub.org/en/setup) covers basic Flatpak setup; some distribution pages also cover graphical installers.

Double-click the downloaded `.flatpak` file again. If **Open With** still appears, select the software center you installed and set it as the default for this file type. Click **Install**. When installation finishes, open the applications menu and search for **FreeShip 26**. If you just installed Flatpak and the app does not appear, sign out of your desktop session and sign back in.
