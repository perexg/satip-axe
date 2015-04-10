#!/bin/sh

###############################################################################
##                SETUP STAPI DEVICE NAME ENVIRONMENT VARIABLES              ##
###############################################################################

# Check if we are on virtual platform (running on x86) or not
# -----------------------------------------------------------
runonvirtualplatform="no"
if [ -f /proc/hce/cwd ]; then
 runonvirtualplatform="yes"
fi

# Verify if root module is specified
# ----------------------------------
export PATH=$PATH:/sbin
if [ -z "$MODULES_INSTALL_DIR" ] ; then
 if [ "$runonvirtualplatform" = "no" ] ; then
  export MODULES_INSTALL_DIR=/lib/modules/axe
 else
  export MODULES_INSTALL_DIR=.
 fi
fi
if [ "$runonvirtualplatform" = "no" ] ; then
 source $MODULES_INSTALL_DIR/load_env.sh
else
 . $MODULES_INSTALL_DIR/load_env.sh
fi

# Check if we are in 32 or 29bits
# -------------------------------
if [ -f $MODULES_INSTALL_DIR/load_modules_list_29BITS.txt ]; then
 LOAD_MODULES_LIST=$MODULES_INSTALL_DIR/load_modules_list_29BITS.txt
 AXE_LOAD_MODULES_LIST=$MODULES_INSTALL_DIR/load_modules_list_axe_29BITS.txt
 MODE="29BITS"
else
 MODE="32BITS"
 if [ -f $MODULES_INSTALL_DIR/load_modules_list_32BITS.txt ]; then
  LOAD_MODULES_LIST=$MODULES_INSTALL_DIR/load_modules_list_32BITS.txt
  AXE_LOAD_MODULES_LIST=$MODULES_INSTALL_DIR/load_modules_list_axe_32BITS.txt
 else
  LOAD_MODULES_LIST=$MODULES_INSTALL_DIR/load_modules_list.txt
  AXE_LOAD_MODULES_LIST=$MODULES_INSTALL_DIR/load_modules_list_axe.txt
 fi
fi

###############################################################################
##                          SETUP DIRECTORY ACCESS                           ##
###############################################################################

# Verify if modules list exist
# ----------------------------
if [ ! -f $LOAD_MODULES_LIST ] ; then
 echo "$LOAD_MODULES_LIST does not exist"
 exit
fi

# Create /dev/stapi directory if not exist
# ----------------------------------------
if [ ! -e /dev/stapi ]; then mkdir -p /dev/stapi ;fi
chmod 755 /dev/stapi
if [ ! -e /dev/axe ]; then mkdir -p /dev/axe ;fi
chmod 755 /dev/axe

###############################################################################
##                          MODULE INSERT PROCEDURE                          ##
###############################################################################

# Return chip version
# -------------------
define_chip_info()
{
 export chipset=`cat /proc/stsys_ioctl/chipset | cut -b1-7`
 export chipver=`cat /proc/stsys_ioctl/chipset | cut -b9-9`
}

# Create a node in /dev/ (Arg 1=node_name)
# ----------------------------------------
create_node()
{
 node_name="$1"
 major=`cat /proc/devices | grep "\b$node_name\b" | cut -b1-3`
 if [ ! -z "$major" ]; then
  dev="/dev/stapi/$node_name"
   if [ -c ${dev} ]; then
    if [ "$runonvirtualplatform" = "no" ] ; then
     cur_major=$((0x`stat -c %t $dev 2>/dev/null`))
    else
     cur_major=`cat /proc/devices | grep $node_name|cut -d " " -f 1`
    fi
    if [ $major -ne $cur_major ]; then
     rm -f ${dev} 2> /dev/null
     mknod $dev c $major 0 || return 1
     chmod 0660 $dev || return 1
    fi
   fi
   if [ ! -c ${dev} ] ; then
    rm -f ${dev} 2> /dev/null
    mknod $dev c $major 0 || return 1
    chmod 0660 $dev || return 1
   fi
 fi
}

# Load firmware (Arg 1=video1 or audio1)
# --------------------------------------
load_firmware()
{
 if [ "$1" = "rt1" ] ; then
  if [ "$chipset" = "STx7108" ] ; then
   cat $MODULES_INSTALL_DIR/rt_7108_firmware.bin >/dev/stapi/lxload
  fi
 fi
 if [ "$1" = "video1" ] ; then
  if [ "$chipset" = "STx5206" ] ; then
   cat $MODULES_INSTALL_DIR/companion_5206_video_Ax.bin >/dev/stapi/lxload
  fi
  if [ "$chipset" = "STx7105" ] ; then
   cat $MODULES_INSTALL_DIR/companion_7105_video_Ax.bin >/dev/stapi/lxload
  fi
  if [ "$chipset" = "STx7106" ] ; then
   cat $MODULES_INSTALL_DIR/companion_7106_video_Ax.bin >/dev/stapi/lxload
  fi
  if [ "$chipset" = "STx7108" ] ; then
   if [ "$chipver" = "A" ] ; then
    cat $MODULES_INSTALL_DIR/companion_7108_video_Ax.bin >/dev/stapi/lxload
   else
    cat $MODULES_INSTALL_DIR/companion_7108_video_Bx.bin >/dev/stapi/lxload
   fi
  fi
  if [ "$chipset" = "STx7109" ] ; then
   cat $MODULES_INSTALL_DIR/companion_7109_video_Cx.bin >/dev/stapi/lxload
  fi
  if [ "$chipset" = "STx7111" ] ; then
   cat $MODULES_INSTALL_DIR/companion_7111_video_Ax.bin >/dev/stapi/lxload
  fi
  if [ "$chipset" = "STx7141" ] ; then
   cat $MODULES_INSTALL_DIR/companion_7141_video_Ax.bin >/dev/stapi/lxload
  fi
  if [ "$chipset" = "STxH205" ] ; then
   cat $MODULES_INSTALL_DIR/companion_h205_video_Ax.bin >/dev/stapi/lxload
  fi
 fi
 if [ "$1" = "audio1" ] ; then
  if [ "$chipset" = "STx5206" ] ; then
   cat $MODULES_INSTALL_DIR/companion_5206_audio.bin >/dev/stapi/lxload
  fi
  if [ "$chipset" = "STx7105" ] ; then
   cat $MODULES_INSTALL_DIR/companion_7105_audio.bin >/dev/stapi/lxload
  fi
  if [ "$chipset" = "STx7106" ] ; then
   cat $MODULES_INSTALL_DIR/companion_7106_audio.bin >/dev/stapi/lxload
  fi
  if [ "$chipset" = "STx7108" ] ; then
   cat $MODULES_INSTALL_DIR/companion_7108_audio.bin >/dev/stapi/lxload
  fi
  if [ "$chipset" = "STx7109" ] ; then
   cat $MODULES_INSTALL_DIR/companion_7109_audio.bin >/dev/stapi/lxload
  fi
  if [ "$chipset" = "STx7111" ] ; then
   cat $MODULES_INSTALL_DIR/companion_7111_audio.bin >/dev/stapi/lxload
  fi
  if [ "$chipset" = "STx7141" ] ; then
   cat $MODULES_INSTALL_DIR/companion_7141_audio.bin >/dev/stapi/lxload
  fi
  if [ "$chipset" = "STxH205" ] ; then
   cat $MODULES_INSTALL_DIR/companion_h205_audio.bin >/dev/stapi/lxload
  fi
 fi
 rmmod lxload
}

# Insert separate STAPI modules
# -----------------------------
insert_separated_modules()
{
cat $LOAD_MODULES_LIST | grep -v "^#" | (while read mode mod_file node_name param; do
 if [ "$mode" != "-" ] ; then
  if [ "$mode" != "$MODE" ] ; then
   continue
  fi
 fi
 if [ "$param" = "-" ] ; then
  param=""
 fi
 if [ ! -f $MODULES_INSTALL_DIR/$mod_file ] ; then
  continue
 fi
 if [ "$node_name" = "lxload" ] ; then
   if [ "$param" = "type=rt1" ] ; then
    if [ ! -f "$MODULES_INSTALL_DIR/rt_7108_firmware.bin" ] ; then
     continue
    fi
   fi
 fi
 if [ "$mod_file" = "embxmailbox.ko" ] ; then
  if [ ! -f $MODULES_INSTALL_DIR/ics.ko ] ; then
   output=$(insmod $MODULES_INSTALL_DIR/$mod_file $param 2>&1 | cat)
  else
   output=$(insmod $MODULES_INSTALL_DIR/$mod_file 2>&1 | cat)
  fi
 else
  output=$(insmod $MODULES_INSTALL_DIR/$mod_file $param 2>&1 | cat)
 fi
 [ "$output" != "" ] && { echo "$output" | grep "File exists" >/dev/null && echo "Warning : $mod_file already inserted, skipping" || { echo "$output" ; exit 1 ; } ; }
 if [ ! "$node_name" = "-" ] ; then
  create_node "$node_name"
 fi
 if [ "$node_name" = "lxload" ] ; then
  define_chip_info
  if [ "$param" = "type=rt1" ] ; then
   load_firmware "rt1"
  fi
  if [ "$param" = "type=video1" ] ; then
   load_firmware "video1"
  fi
  if [ "$param" = "type=video2" ] ; then
   load_firmware "video2"
  fi
  if [ "$param" = "type=audio1" ] ; then
   load_firmware "audio1"
  fi
  if [ "$param" = "type=audio2" ] ; then
   load_firmware "audio2"
  fi
 fi
 done)
}

# Insert unique STAPI module
# --------------------------
insert_unified_module()
{
 # Now we load STAPI and create nodes
 echo "* Inserting STAPI Module"
 insmod $MODULES_INSTALL_DIR/stapi_core_stripped.ko || return 1
 if [ "$1" != "no_stapi_ioctl" ] ; then
  insmod $MODULES_INSTALL_DIR/stapi_ioctl_stripped.ko || return 1
 else
  if [ -f $MODULES_INSTALL_DIR/stpti4_ioctl.ko ]; then
   insmod $MODULES_INSTALL_DIR/stpti4_ioctl.ko || return 1
  fi
  if [ -f $MODULES_INSTALL_DIR/stpti5_ioctl.ko ]; then
   insmod $MODULES_INSTALL_DIR/stpti5_ioctl.ko || return 1
  fi
 fi
 if [ -f $MODULES_INSTALL_DIR/staudlx_alsa.ko ]; then
  insmod $MODULES_INSTALL_DIR/staudlx_alsa.ko || return 1
 fi
 if [ "$1" != "no_stapi_ioctl" ] ; then
  echo "* Creating device nodes in /dev/stapi"
  cat $LOAD_MODULES_LIST | grep -v "^#" | grep -v lxload | (while read mode mod_file node_name param; do
  if [ ! "$node_name" = "-" ] ; then
   create_node $node_name
  fi
  done)
 fi
}


# Inserting AXE modules
# ---------------------
insert_axe_modules  ()
{
 local HW=$(awk 'match($0, /hw=[^ ]*/) {print substr($0,RSTART,RLENGTH)}' /proc/cmdline)
 local HWID=${HW:3:40}

 #echo "$HW"
 #echo "$HWID"

 if [ ${#HWID} -ne 32 ] ; then
  echo "HWID has wrong length"
 fi

 grep -v "^#" $AXE_LOAD_MODULES_LIST | (while read mode mod_file node_name param minor_cnt; do
 if [ "$mode" != "-" ] ; then 
  if [ "$mode" != "$MODE" ] ; then
   continue
  fi
 fi
 if [ ! -f $MODULES_INSTALL_DIR/$mod_file ] ; then
  continue
 fi
 if [ "$param" = "-" ] ; then
  param=""
 fi

 if [ "$node_name" = "frontend" -a "${HWID:0:3}" = "161" ] ; then
   mod_file="axe_fe_ml.ko"
 fi

 param="$(echo $param | sed -e 's/,/ /g')"
 insmod $MODULES_INSTALL_DIR/$mod_file $param
 done)

 /lib/modules/axe/mknodes.out i

 ## WORKAROUND: if no HWID defined reverse the demux's routing
 if [ ${#HWID} -eq 0 ] ; then
  echo "SWAPPING TS ROUTE LIKE FOR OLD BOARD"
  mv /dev/axe/demux-0 /dev/axe/demux-00
  mv /dev/axe/demux-1 /dev/axe/demux-0
  mv /dev/axe/demux-00 /dev/axe/demux-1

  mv /dev/axe/demuxts-0 /dev/axe/demuxts-00
  mv /dev/axe/demuxts-1 /dev/axe/demuxts-0
  mv /dev/axe/demuxts-00 /dev/axe/demuxts-1

  mv /dev/axe/demux-2 /dev/axe/demux-20
  mv /dev/axe/demux-3 /dev/axe/demux-2
  mv /dev/axe/demux-20 /dev/axe/demux-3

  mv /dev/axe/demuxts-2 /dev/axe/demuxts-20
  mv /dev/axe/demuxts-3 /dev/axe/demuxts-2
  mv /dev/axe/demuxts-20 /dev/axe/demuxts-3
 fi
}


# Choose between inserting severall or unified module
# ---------------------------------------------------
if [ $# -eq 0 ] || [ "$1" != "not_unified" ]; then
 if [ -f "$MODULES_INSTALL_DIR/stapi_core_stripped.ko" ] && [ -f "$MODULES_INSTALL_DIR/stapi_ioctl_stripped.ko" ]; then
  insert_unified_module $1
 else
  insert_separated_modules
 fi
else
 insert_separated_modules
fi
insert_axe_modules

