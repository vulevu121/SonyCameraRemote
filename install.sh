sudo apt install autoconf libtool libudev-dev gcc g++ make cmake unzip libxml2-dev
sed -i 's/"quiet splash"/"quiet splash usbcore.usbfs_memory_mb=150"/g' /etc/default/grub
sudo upgrate-grub
echo "Please reboot system"
