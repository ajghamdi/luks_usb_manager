# LUKS USB Manager

This an attempt to write a simple and secure **C program** to automate some operations on **LUKS-encrypted USB drive** on Linux. This code makes it easy to **unlock, mount, lock, and unmount** the usb drive while ensuring **data security and passphrase protection**.
*Improvements will be introduced as needed*.

---

## Features
- **Automated LUKS Operations** – Unlock, mount, lock, and unmount your USB drive with a single command.
- **Secure Passphrase Handling** – Wipes passphrase from memory to prevent leaks.
- **No Shell Commands** – Uses only C libraries (`libcryptsetup`, `libudev`, system calls).
- **USB Device Portability** – Manage the LUKS and USB devices by UUID, which allow using the USB on multiple Linux machines.
- **Data Integrity** – Flushes disk writes before unmounting to prevent corruption.

---

## Dependencies

- **GCC (GNU Compiler Collection)**
  - Install on Arch-based distros (Arch Linux, Manjaro):
    ```bash
    sudo pacman -S gcc
    ```
  - Install on Debian-based distros (Debian, Ubuntu, Linux Mint, Pop!_OS, etc.):
    ```bash
    sudo apt install gcc
    ```
  - Install on Fedora-based distros (Fedora, RHEL, AlmaLinux, Rocky Linux, etc.):
    ```bash
    sudo dnf install gcc
    ```
  - Install on OpenSUSE-based distros:
    ```bash
    sudo zypper install gcc
    ```

- **libcryptsetup (LUKS Management Library)**
  - Install on Arch-based distros (Arch Linux, Manjaro):
    ```bash
    sudo pacman -S cryptsetup
    ```
  - Install on Debian-based distros (Debian, Ubuntu, Linux Mint, Pop!_OS, etc.):
    ```bash
    sudo apt install libcryptsetup-dev
    ```
  - Install on Fedora-based distros (Fedora, RHEL, AlmaLinux, Rocky Linux, etc.):
    ```bash
    sudo dnf install cryptsetup-libs cryptsetup-devel
    ```
  - Install on OpenSUSE-based distros:
    ```bash
    sudo zypper install libcryptsetup-devel
    ```
    
- **libudev (Device Management Library)**
  - Install on Arch-based distros (Arch Linux, Manjaro):
    ```bash
    sudo pacman -S systemd-libs
    ```
  - Install on Debian-based distros (Debian, Ubuntu, Linux Mint, Pop!_OS, etc.):
    ```bash
    sudo apt install libudev-dev
    ```
  - Install on Fedora-based distros (Fedora, RHEL, AlmaLinux, Rocky Linux, etc.):
    ```bash
    sudo dnf install systemd-devel
    ```
  - Install on OpenSUSE-based distros:
    ```bash
    sudo zypper install systemd-devel
    ```

---

## Compilation & Installation
Manual compilation is needed using `gcc`:

```bash
gcc luks_usb_manager.c -o luks-usb-manager -lcryptsetup -ludev
```

To run the program, use `sudo`, as LUKS operations require root permissions:

```bash
sudo ./luks-usb-manager
```

---

## Usage
Run the compiled binary using `sudo`, and it will:
- If the USB drive is *locked*:
  - **Ask for passphrase**.
  - **Unlock the drive**
  - **Securely clear the passphrase from the memory**
  - **Mount the drive**
- If the USB drive is *unlocked*:
  - **Force the OS to write all cached data to disk**.
  - **Unmount the drive**.
  - **Lock the drive**

Run command:
```bash
sudo ./luks-usb-manager
```

### Example Output

- If LUKS device is locked:
```
Partition is locked. Unlocking now...
Enter LUKS passphrase: ********
USB unlocked and mounted at /mnt/encrypted_usb
Press ENTER to exit...
```

- If LUKS device is unlocked:
```
Partition is already unlocked. Locking it now...
Flushing write buffers...
USB locked and unmounted successfully.
Press ENTER to exit...
```

---

## License
This project is licensed under the **MIT License** – feel free to use, modify, and contribute.

---

## Contributions
Pull requests are welcome. If you find any bugs or have feature suggestions, feel free to open an **issue**.

---

## Author
`Ahmed J Alghamdi / ajghamdi`
[luks_usb_manager on GitHub](https://github.com/ajghamdi/luks_usb_manager)

---

### Tests
Tested on:
- OS: `Arch Linux 6.13.4-arch1-1 x86_64`
- GCC version: `14.2.1 20250207`
- systemd-libs version: `257.3-1`
- cryptsetup version: `2.7.5-2`

---
