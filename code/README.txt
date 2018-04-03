################################################################
This README covers the step to setup a PC as an edges digitizer.  
JDB 2013-11-06
################################################################

1. Do fresh install of Ubuntu 10.04.4 LTS


2. Setup network configuration:
	hostname: edges-pc
	IP: 172.20.1.251
	Subnet mask: 255.255.255.0
	Gateway: 172.20.1.1
	DNS: 192.231.203.132, 192.231.203.3


3. Update packages using update manager. [Do not upgrade, Signatec needs kernel 2.6.x and later Ubuntu version use kernel 3.x.x]


4. Turn OFF updating checking using update manager settings


5. Mount /disk1 using parition's UUID (this is better than adding /dev/sda3 because the /dev mappings sometimes change)

>> sudo mkdir /disk1
>> ls /dev/disk/by-uuid/ -alh
>> sudo nano /etc/fstab  %% Add line
>> sudo mount -a 


6. Add openssh-server

>> sudo apt-get -f install openssh-server


7.  Add freenx following instructions from:
http://www.humans-enabled.com/2012/05/how-to-install-freenx-on-ubuntu-1204.html

>> sudo add-apt-repository ppa:freenx-team
>> sudo apt-get update && sudo apt-get install freenx
>> wget https://bugs.launchpad.net/freenx-server/+bug/576359/+attachment/1378450/+files/nxsetup.tar.gz && tar xvf nxsetup.tar.gz && sudo cp nxsetup /usr/lib/nx 
>> sudo /usr/lib/nx/nxsetup --install --setup-nomachine-key
>> echo -e "\n#Use unity 2d for client sessions\nCOMMAND_START_GNOME='gnome-session --session=ubuntu-2d'"|sudo tee -a /etc/nxserver/node.conf 
>> sudo /etc/init.d/freenx-server restart


8. Find the base address of the parallel port and edit the parallelport.c file to update.  See the instructions in the parallelport.c file.


9. Compile the Signate driver following README directions at:

>> cd disk1/pkg/Signatec_PX14400_ADC_Card/PX14400
>> sudo apt-get install build-essential
>> sudo apt-get install libxml2-dev
>> sudo make
>> sudo make install
>> sudo cp load_scripts/px14400_load /usr/local/bin/

Add "/usr/local/bin/px14400_load" before the exit line of /usr/rc.local


10. Install the Labjack U3 usb drivers following the instructions at: 
http://labjack.com/support/linux-and-mac-os-x-drivers

>> cd /disk1/pkg/Labjack_U3
>> sudo apt-get install build-essential
>> sudo apt-get install libusb-1.0-0-dev 
>> sudo apt-get install git-core
>> git clone git://github.com/labjack/exodriver.git
>> cd exodriver/
>> sudo ./install.sh
>> cd ..
>> git clone git://github.com/labjack/LabJackPython.git
>> cd ../LabJackPython/
>> sudo python setup.py install
>> python
	>>> import u3
	>>> d = u3.U3()
	>>> d.configU3()

11. Install GTK2.0 used by pxspec on-screen plotting

>> sudo apt-get install libgtk2.0-dev


12. Install FFT3 library used by pxspec

>> sudo apt-get install libfftw3-dev 


13.  Compile EDGES code

>> cd /disk1/edges/code/
>> sudo ./pxspecmake









