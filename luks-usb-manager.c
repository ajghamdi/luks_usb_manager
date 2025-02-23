#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <libcryptsetup.h>
#include <libudev.h>
#include <limits.h>
#include <termios.h>

#define FILESYSTEM_UUID "ada0de09-6103-4216-93f5-c6a7b02f3248"  // UUID of the filesystem inside LUKS
#define LUKS_DEVICE_UUID "b472bee4-4c67-4103-b491-c1b3e869cb81" // UUID of the LUKS partition
#define MAPPER_NAME "encrypted_usb"                            // Name of the LUKS mapped device
#define MAPPED_DEVICE "/dev/mapper/encrypted_usb"              // Path to the mapped LUKS device
#define MOUNT_POINT "/mnt/encrypted_usb"                       // Mount point for the decrypted partition
#define MAX_PASS_LEN 256                                       // Maximum passphrase length
#define FILESYSTEM_TYPE "xfs"                                  // Filesystem type (adjust if using another format like '"ext4"')

// Pause execution and waits for the user to press ENTER.
void wait_for_user() {
    printf("\nPress ENTER to exit...\n");
    getchar(); // Wait for user input (ENTER key)
}

// Read a passphrase from the user without displaying it on the screen.
void get_hidden_passphrase(char *passphrase, size_t size) {
    struct termios oldt, newt; // Structures to store terminal settings

    // Step 1: Retrieve the current terminal settings
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) { // Get the current terminal attributes and stores them in 'oldt'.
        fprintf(stderr, "Error: Failed to get terminal attributes.\n");
        return;
    }

    // Step 2: Create a modified version of the terminal settings
    newt = oldt; // Copy the current settings into 'newt' so we can modify them.

    // Step 3: Disable echo mode
    newt.c_lflag &= ~ECHO; // This prevents the user's input from being displayed on the screen while typing.

    // Step 4: Apply the modified settings
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) != 0) { // Update the terminal settings to hide the input.
        fprintf(stderr, "Error: Failed to disable terminal echo.\n");
        return;
    }

    // Step 5: Prompt the user for the passphrase
    printf("Enter LUKS passphrase: ");

    // Step 6: Read the passphrase from standard input
    if (fgets(passphrase, size, stdin) == NULL) { // Read the passphrase safely, preventing buffer overflows.
        fprintf(stderr, "Error: Failed to read passphrase.\n");
    }

    // Step 7: Remove any trailing newline character from the input
    passphrase[strcspn(passphrase, "\n")] = '\0'; // Find the first occurrence of '\n' and replaces it with '\0'

    // Step 8: Restore the original terminal settings
    if (tcsetattr(STDIN_FILENO, TCSANOW, &oldt) != 0) { // Bring back normal behavior (input is visible again).
        fprintf(stderr, "Error: Failed to restore terminal settings.\n");
    }

    // Step 9: Print a newline to move to the next line cleanly
    printf("\n");
}

// Retrieve the device path for a given UUID using 'libudev'.
int get_device_by_uuid(const char *uuid, char *device_path, size_t size) {
    struct udev *udev = udev_new(); // Initialize 'udev' context, which allows to query system devices.
    if (!udev) { // If initialization fails, print an error and return failure.
        fprintf(stderr, "Error: Failed to initialize udev.\n");
        return -1;
    }

    struct udev_enumerate *enumerate = udev_enumerate_new(udev); // Create a new 'udev_enumerate' object to search for devices.
    if (!enumerate) { // If enumeration fails, clean up and return failure.
        fprintf(stderr, "Error: Failed to create udev enumerate object.\n");
        udev_unref(udev);
        return -1;
    }

    udev_enumerate_add_match_property(enumerate, "ID_FS_UUID", uuid); // Filter the search to only include devices with the given UUID.

    udev_enumerate_scan_devices(enumerate); // Scan the system for matching devices.

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate); // Get the list of devices that matched the UUID.
    struct udev_list_entry *entry;

    // Iterate through the list of matching devices.
    udev_list_entry_foreach(entry, devices) {
        const char *path = udev_list_entry_get_name(entry); // Get the system path for the device.

        struct udev_device *dev = udev_device_new_from_syspath(udev, path); // Create a 'udev_device' object from the system path.
        if (!dev) { // If device creation fails, continue to the next entry.
            continue;
        }

        const char *devnode = udev_device_get_devnode(dev); // Get the actual device node (e.g., '/dev/sdb1').

        if (devnode) { // If the device node exists, copy it to 'device_path'.
            strncpy(device_path, devnode, size); // Copy the path safely.

            // Clean up before returning.
            udev_device_unref(dev);
            udev_enumerate_unref(enumerate);
            udev_unref(udev);
            return 0; // Success: Device found!
        }

        udev_device_unref(dev); // If 'devnode' was NULL, free the device and continue searching.
    }

    // If no matching device was found, clean up resources.
    udev_enumerate_unref(enumerate);
    udev_unref(udev);

    return -1; // Return failure since no device matched the UUID.
}

// Check if the LUKS partition is currently unlocked.
int is_unlocked() {
    struct stat st; // Structure to hold file status information

    /*
    * Use 'stat()' to check if the mapped LUKS device exists:
    * - If 'stat()' returns 0, the file exists (partition is unlocked).
    * - If 'stat()' fails, the device is missing (partition is locked).
    */
    return (stat(MAPPED_DEVICE, &st) == 0);
}

// Unlock a LUKS-encrypted partition and mounts it.
void unlock_partition() {
    struct crypt_device *cd; // A handle for the LUKS device
    char luks_device[PATH_MAX] = {0}; // A buffer to store the LUKS device path
    char passphrase[MAX_PASS_LEN] = {0}; // A buffer for user-provided passphrase

    // Step 1: Find the LUKS device by UUID
    if (get_device_by_uuid(LUKS_DEVICE_UUID, luks_device, sizeof(luks_device)) != 0) {
        fprintf(stderr, "Error: LUKS device not found.\n");
        wait_for_user(); // Pause before exiting
        return;
    }

    // Step 2: Initialize cryptsetup for the device
    // Create a new crypt device context for the LUKS partition, then
    // load the LUKS header to prepare for unlocking.
    if (crypt_init(&cd, luks_device) < 0 || crypt_load(cd, CRYPT_LUKS2, NULL) < 0) {
        fprintf(stderr, "Error: Failed to initialize cryptsetup.\n");
        wait_for_user();
        return;
    }

    // Step 3: Prompt the user for the LUKS passphrase
    get_hidden_passphrase(passphrase, sizeof(passphrase));

    // Step 4: Attempt to unlock the LUKS partition
    if (crypt_activate_by_passphrase(cd, MAPPER_NAME, CRYPT_ANY_SLOT, passphrase, strlen(passphrase), 0) < 0) {
        fprintf(stderr, "Error: Incorrect LUKS passphrase.\n");
        crypt_free(cd); // Cleanup cryptsetup context
        wait_for_user();
        return;
    }

    crypt_free(cd); // Free cryptsetup context after unlocking
    
    // {{SECURELY ERASE PASSPHRASE FROM MEMORY}}
    #ifdef __GLIBC__
        explicit_bzero(passphrase, sizeof(passphrase)); // Best way to securely clean memory
    #else
        volatile char *p = passphrase;
        while (*p) *p++ = 0; // Securely overwrite memory if GLIBC is not available
    #endif

    // Step 5: Ensure the mount point exists
    mkdir(MOUNT_POINT, 0755); // Create the mount directory if it doesn't exist with permissions: rwxr-xr-x

    // Step 6: Mount the decrypted partition
    if (mount(MAPPED_DEVICE, MOUNT_POINT, FILESYSTEM_TYPE, MS_RELATIME, NULL) == 0) {
        printf("USB unlocked and mounted at %s\n", MOUNT_POINT);
    } else {
        perror("Failed to mount the unlocked partition"); // Print system error message
    }

    wait_for_user(); // Pause before exiting
}

// Lock the LUKS-encrypted partition and unmounts it safely.
void lock_partition() {
    struct crypt_device *cd; // A handle for the LUKS device

    // Step 1: Check if the LUKS partition is already locked
    if (!is_unlocked()) { // If the device is locked, there is nothing to do.
        printf("LUKS partition is already locked.\n");
        wait_for_user(); // Pause before exiting
        return;
    }

    // Step 2: Flush any pending disk writes to prevent data loss
    printf("Flushing write buffers...\n");
    sync(); // Force the OS to write all cached data to disk.

    // Step 3: Unmount the partition
    if (umount(MOUNT_POINT) != 0) { // Detache the filesystem from the mount point.
        perror("Error: Failed to unmount the partition"); // Print error if the unmount operation fails.
        wait_for_user();
        return;
    }

    // Step 4: Lock (deactivate) the LUKS device
    if (crypt_init_by_name(&cd, MAPPER_NAME) == 0) { // Initialize a crypt device context using its mapper name.
        crypt_deactivate(cd, MAPPER_NAME); // Deactivate the LUKS-mapped device and lock it.
        crypt_free(cd); // Free the cryptsetup context.
        printf("USB locked and unmounted successfully.\n");
    } else {
        fprintf(stderr, "Failed to lock the LUKS partition.\n");
    }

    wait_for_user(); // Pause before exiting
}


// Main function for managing the LUKS-encrypted partition.
int main() {
    if (is_unlocked()) { // Determine if the partition is currently mounted and decrypted.
        printf("Partition is already unlocked. Locking it now...\n");
        lock_partition(); // Safely unmount and lock the device
    } else {
        printf("Partition is locked. Unlocking now...\n");
        unlock_partition(); // Prompt for passphrase and unlock the device
    }

    return 0; // Return 0 indicating normal program termination
}
