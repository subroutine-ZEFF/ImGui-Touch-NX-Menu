#----------------------------- User configuration -----------------------------

# Common settings
#------------------------

# How you're loading your module. Used to determine how to find the target module. (AsRtld/Module/Kip)
LOAD_KIND := Module

# Program you're targetting. Used to determine where to deploy your files.
# 0100D71004694000 = Minecraft (Bedrock) on Nintendo Switch.
PROGRAM_ID := 0100D71004694000

# Optional path to copy the final ELF to, for convenience.
ELF_EXTRACT :=

# Python command to use. Must be Python 3.4+.
PYTHON := python3

# JSON to use to make .npdm
NPDM_JSON := application.json

# Additional C/C++ flags to use.
# -O2/-fno-shrink-wrap work around internal compiler errors in devkitA64 GCC 15.2.0.
C_FLAGS := -O2 -fno-shrink-wrap -pipe
CXX_FLAGS := -fno-shrink-wrap

# AsRtld settings
#------------------------

# Path to the SD card. Used to mount and deploy files on SD, likely with hekate UMS.
MOUNT_PATH := /mnt/k

# Module settings
#------------------------

# Settings for deploying over FTP. Used by the deploy-ftp.py script.
FTP_IP := 192.168.1.100
FTP_PORT := 5000
FTP_USERNAME := anonymous
FTP_PASSWORD :=

# Settings for deploying to Ryu. Used by the deploy-ryu.sh script.
RYU_PATH := /mnt/c/Users/<user>/AppData/Roaming/Ryujinx
