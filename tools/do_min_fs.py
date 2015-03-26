#! /usr/bin/python

import string
import sys, os, re, getopt

#-----------------------------------------------------
def run_cmd(cmd):
    """ runs the os.system(). run a command into the shell and return
        the stdout on a list. Each element terminates whith '\n'
        It doesn't catch errors
        You can use the builtin 'command' module.
        Like: command.getoutput()
    """
    try:
      cmd = str(cmd) + '  > output.txt'
      # print 'command: ' + cmd
      os.system(cmd)
      f=open('output.txt','rw')
      cmd_output = f.readlines()
      f.close()
      os.system('rm output.txt')
      # print 'cmd_output: ' + str(cmd_output[0][:-1])
      # print 'cmd_output: ' + str(cmd_output)
    except:
         print '-> Error run_cmd'
    else:
        return cmd_output

#-----------------------------------------------------------------

def get_cmd_output(cmd):
    """ run a command on the shell and return
        stdout and stderr on the same list.
        Each element isn't '/n' terminated
        os.popen* is obsolete! it's replaced by the new subprocess module (2.6)
        FIXME
    """
    #p = Popen(cmd, shell=True, bufsize=bufsize, stdin=PIPE, stdout=PIPE, stderr=SDTOUT,
    #close_fds=True)
    #(dummy, stdout_and_stderr) = (p.stdin, p.stdout)
    (dummy, stdout_and_stderr) = os.popen4(cmd, 'w')
    result = stdout_and_stderr.read()
    return  result.splitlines()

#-----------------------------------------------------------------

def get_cmd_output_2(cmd):
    """ run a command on the shell and return
        stdout and stderr as separate list items. result[0]=out result[1]=err
        os.popen* is obsolete! it's replaced by the new subprocess module (2.6)
        FIXME
    """

    #p = Popen(cmd, shell=True, bufsize=bufsize, stdin=PIPE, stdout=PIPE, stderr=PIPE,
    #close_fds=True)
    #(stdin, stdout, stderr) = (p.stdin, p.stdout, p.stderr)
    (stdin, stdout, stderr) = os.popen3(cmd, 'w')
    out = stdout.read()
    err = stderr.read()
    result = ['' ,'']
    result[0] = out
    result[1] = err
    return  result

#-----------------------------------------------------------------

def unique(s):
    """ Return a list of elements without duplicates
        Imput: list
	Output: the same list (without duplicates)
    """
    n = len(s)
    if n == 0:
       return []

    # First Try to use a dictionary
    # If it doesn't work, it will fail quickly, so it
    # doesn't cost much to try it.  It requires that all the
    # sequence elements be hashable, and support equality comparison.

    u = {}
    try:
       for x in s:
           u[x] = 1
    except TypeError:
           del u   # move to the next method
    else:
        return u.keys()

    # We can't hash all the elements. Second fastest is to sort
    # which brings the equal elements together; then duplicates are
    # easy to weed out in a single pass.
    # NOTE:  Python's list.sort() was designed to be efficient in the
    # presence of many duplicate elements.  This isn't true of all
    # sort functions in all languages or libraries, so this approach
    # is more effective in Python than it may be elsewhere.

    try:
       t = list(s)
       t.sort()
    except TypeError:
       del t    # move to the next method
    else:
       assert n > 0
       last = t[0]
       lasti = i = 1
       while i < n:
           if t[i] != last:
              t[lasti] = last = t[i]
              lasti += 1
           i += 1
       return t[:lasti]

    # Brute force
    u = []
    for x in s:
      if x not in u:
         u.append(x)
    return u

#------------------------------------------------

def get_lib_path(list):
    """extract from the ldd command output, only the library path
       information.
       Input: a list containing the full ldd results
       Output: a list containing only the library paths
    """
    try:
      lib_paths = []
      #print str(list)
      for i in range(len(list)):
        a = list[i]
        if(a.find('=>') >= 0):
           start = a.find('=>') + 3
           end = a.find('(0x0') - 1
           #if (a.find('=> not found (0') >= 0 or (not a.find('/', start)>= 0) ):
           if (a.find('=> not found (0') >= 0 ):
             print '\n' + 35*'=='
             print 'Warning:\n  ldd was not able to locate the full path of the following library :'
             print str(a) + '\n' + ' Please, retreive it and add the path of those dir in PATH var'
             print 35*'=='
           #b = target_prefix + a[start:end]
           b = a[start:end]
           # try: busybox, LDD_ROOT_BASE=target_prefix, LDD_ROOT_SHOW not defined. If you define it
           # (1 or 0) it seems don't catch some libs (2 verify: xterm)
           if b.find('/opt/') == -1:
              b = target_prefix + b
           print '\tlibrary: ' + str(b)
           lib_paths.append(b)
    except:
         print '-> Error from get_lib_path'
    else:
       return lib_paths

#------------------------------------------------

def del_line_feed(my_list):
    """ clear the '\n' from the given list;
        return a second list without '\n'
    """
    my_list2 = []
    for item in my_list:
      my_list2.append(re.sub('\n', '', item))
    return my_list2

#---------------------------------------

def setup_busybox():
    cmd = 'cp ' + target_prefix + '/bin/busybox fs/bin'
    run_cmd(cmd)
    busybox_cmds = ['sh', 'ls', 'echo', 'mount', 'umount', 'pwd', 'mv', 'cp', 'rm', 'ln', \
    'mkdir', 'vi', 'cat', 'halt']
    for a in busybox_cmds:
      cmd = ' ln -s  /bin/busybox  fs/bin/' + str(a)
      run_cmd(cmd)
      print cmd

    cmd = 'ln -s  /bin/busybox  fs/sbin/init'
    run_cmd(cmd)
    print cmd

    # init.d/rcS
    fd = open('fs/etc/init.d/rcS', 'wr')
    fd.write('#!/bin/bash\n echo "Welcome to a custom minimal file system"\n mount -t proc proc /proc\n mount -o remount,noatime /dev/root /')
    fd.close()
    cmd = ' chmod a+x fs/etc/init.d/rcS '
    run_cmd(cmd)
    print cmd

    # init.d/rcSBB
    fd2 = open('fs/etc/init.d/rcSBB', 'wr')
#    fd2.write('#!/bin/sh\n # example rcS script\n echo "Welcome to STLinux!"\n mount -t proc proc /proc\n mount -n -o remount,rw /\n mount -t devpts none /dev/pts -ogid=5,mode=620\n /usr/sbin/telnetd -l /bin/sh')
    fd2.write('#!/bin/sh\n # example rcS script\n echo "Welcome to STLinux!"\n mount -t proc proc /proc\n mount -n -o remount,rw /\n mount -t devpts none /dev/pts -ogid=5,mode=620\n  /bin/sh')
    fd2.close()
    cmd = ' chmod a+x fs/etc/init.d/rcSBB '
    run_cmd(cmd)
    print cmd

    cmd = ' chmod a+x fs/etc/init.d/rcSBB '
    run_cmd(cmd)
    print cmd

#-----------------------------------------------

def setup_sysvinit():
    print 'setup_sysvinit target_prefix ' + str(target_prefix)
    bin = ['/usr/bin/passwd', '/bin/egrep']
    for j in bin:
        cmd = 'cp  ' + target_prefix + j + ' fs' + j
        run_cmd(cmd)
        print cmd

    # link sh --> bash
    cmd = ' cd fs/bin; ln -s  bash  sh'
    get_cmd_output(cmd)

#    run_cmd('cp -a ' + target_prefix + '/etc/inittab' + ' fs/etc/')
#    run_cmd('cp -a ' + target_prefix + '/etc/inittabBB' + ' fs/etc/')
#    run_cmd('cp -a ' + target_prefix + '/etc/login.access' + ' fs/etc/')
#    run_cmd('cp -a ' + target_prefix + '/etc/login.defs' + ' fs/etc/')
    run_cmd('cp -a ' + target_prefix + '/etc/fstab' + ' fs/etc/')
    run_cmd('cp -a ' + target_prefix + '/etc/passwd' + ' fs/etc/')
    run_cmd('cp -r ' + target_prefix + '/etc/mtab' + ' fs/etc/')
    run_cmd('mkdir -p fs/etc/default')
    run_cmd('cp -r ' + target_prefix + '/etc/default/*' + ' fs/etc/default')

    run_cmd('mkdir -p fs/etc/init.d')
    # copy the full init.d
    #run_cmd('cp -r ' + target_prefix + '/etc/init.d/' + ' fs/etc/')

    # dnsmasq console-screen.sh messagebus hwclock.sh rdisc ifupdown procps.sh
    # device-mapper nfs-kernel-server nfs-common
    init_files = ['bootlogd', 'syslogd', 'bootmisc.sh', 'rcSBB', 'umountfs', \
    'checkfs.sh',  'klogd',  'ntpdate',  'umountnfs.sh', \
    'checkroot.sh', 'makedev',  'nviboot', 'rmnologin', 'urandom', \
    'portmap', 'sendsigs', 'mountall.sh', 'setserial', 'mountnfs.sh', 'rc', \
    'single', 'hostname.sh', 'rcS', 'syslog', 'bootclean.sh', 'mountvirtfs']
    for i in init_files:
        run_cmd('cp -r ' + target_prefix + '/etc/init.d/' + i + ' fs/etc/init.d/')

    run_cmd('mkdir -p fs/etc/rc.d/rc0.d')
    run_cmd('mkdir -p fs/etc/rc.d/rc1.d')
    run_cmd('mkdir -p fs/etc/rc.d/rc2.d')
    run_cmd('mkdir -p fs/etc/rc.d/rc3.d')
    run_cmd('mkdir -p fs/etc/rc.d/rc4.d')
    run_cmd('mkdir -p fs/etc/rc.d/rc5.d')
    run_cmd('mkdir -p fs/etc/rc.d/rc6.d')
    run_cmd('mkdir -p fs/etc/rc.d/rcS.d')
    run_cmd('cp -dr ' + target_prefix + '/etc/rc.d/rc0.d/* ' + ' fs/etc/rc.d/rc0.d')
    run_cmd('cp -r ' + target_prefix + '/etc/rc.d/rc1.d/* ' + ' fs/etc/rc.d/rc1.d')
    run_cmd('cp -r ' + target_prefix + '/etc/rc.d/rc2.d/* ' + ' fs/etc/rc.d/rc2.d')
    run_cmd('cp -r ' + target_prefix + '/etc/rc.d/rc3.d/* ' + ' fs/etc/rc.d/rc3.d')
    run_cmd('cp -r ' + target_prefix + '/etc/rc.d/rc4.d/* ' + ' fs/etc/rc.d/rc4.d')
    run_cmd('cp -r ' + target_prefix + '/etc/rc.d/rc5.d/* ' + ' fs/etc/rc.d/rc5.d')
    run_cmd('cp -r ' + target_prefix + '/etc/rc.d/rc6.d/* ' + ' fs/etc/rc.d/rc6.d')
    run_cmd('cp -r ' + target_prefix + '/etc/rc.d/rcS.d/* ' + ' fs/etc/rc.d/rcS.d')
    run_cmd('cp -r ' + target_prefix + '/etc/rc.d/init.d '  + ' fs/etc/rc.d/')

    # link init.d  rc.d
    #cmd = ' cd fs/etc/rc.d/; ln -s  ../init.d  init.d'
    #get_cmd_output(cmd)

    cmd = ' chmod a+x fs/lib/* '
    run_cmd(cmd)
    print cmd

    cmd = ' chmod a+x fs/usr/lib/* '
    run_cmd(cmd)
    print cmd

    cmd = 'cp  -d ' + target_prefix + '/usr/lib/libwrap*' + ' fs/usr/lib/'
    run_cmd(cmd)
    print cmd
    cmd = 'cp  -d ' + target_prefix + 'lib/libnsl*' + ' fs/usr/lib/'
    run_cmd(cmd)
    print cmd

#------------------------------------------------

def  make_dev():
     device_list = ['console c 5 1 ' , 'null c 1 3 ', 'fb0 c 29 0 ', 'fbcondecor c 10 63 ', \
     'ram0 b 1 0 ', 'ram1 b 1 1 ', 'ram2 b 1 2 ', 'ram3 b 1 3 ', 'ram4 b 1 4 ', 'ram5 b 1 5 ', \
     'ram6 b 1 6 ', 'ram7 b 1 7 ', 'ram8 b 1 8 ', 'ram9 b 1 9 ', 'ram10 b 1 10 ', 'ram11 b 1 11 ', \
     'ram12 b 1 12 ', 'ram13 b 1 13 ', 'ram14 b 1 14 ', 'ram15 b 1 15 ', 'ram16 b 1 16 ', \
     'rtc c 10 135 ', 'sda1 b 8 1 ', 'timer c 116 33 ', 'tty c 5 0 ', 'tty0 c 4 0 ', \
     'tty1 c 4 1 ', 'tty2 c 4 2 ', 'tty10 c 4 10 ', 'tty16 c 4 16 ']

     for i in device_list:
         cmd = 'mknod fs/dev/' + i
         print cmd
         run_cmd(cmd)

     run_cmd(' ln -s fs/dev/ram1 fs/dev/ram ')

#------------------------------------------------

def gen_fs(lib_list, init_type):
    """ 1) generate a minimal FS skeleton;
        2) get paths from lib_list.
        Copy all files into the fs. Setup busybox or sh shell
    """
    print '\t coping libraries  and binary files \n'
    run_cmd('rm -rf fs fs.cpio')
    run_cmd('mkdir -p fs/sbin')
    run_cmd('mkdir -p fs/bin')
    run_cmd('mkdir -p fs/dev')
    run_cmd('mkdir -p fs/etc')
    run_cmd('mkdir -p fs/lib')
    run_cmd('mkdir -p fs/tmp')
    run_cmd('mkdir -p fs/proc')
    run_cmd('mkdir -p fs/usr')
    run_cmd('mkdir -p fs/var')


    for i in lib_list:
        target_dir = os.path.dirname(i)
        file_name = os.path.basename(i)

        if (target_dir.find('/sbin') >=0):
           fs_dir = target_dir.replace(target_prefix, '')
           run_cmd('mkdir -p ' + 'fs/' + fs_dir)
           run_cmd('cp -a ' + i + ' fs/' + fs_dir)
           print 'cp -a ' + i + ' fs/' + fs_dir
        if (target_dir.find('/bin') >=0):
           fs_dir = target_dir.replace(target_prefix, '')
           run_cmd('mkdir -p ' + 'fs/' + fs_dir)
           run_cmd('cp -a ' + i + ' fs/' + fs_dir)
           #run_cmd('cp ' + i + ' fs/' + fs_dir)
        if (target_dir.find('/dev') >=0):
           fs_dir = target_dir.replace(target_prefix, '')
           run_cmd('mkdir -p ' + 'fs/' + fs_dir)
           run_cmd('cp ' + i + ' fs/' + fs_dir)
        if (target_dir.find('/etc') >=0):
           fs_dir = target_dir.replace(target_prefix, '')
           run_cmd('mkdir -p ' + 'fs/' + fs_dir)
           run_cmd('cp ' + i + ' fs/' + fs_dir)
        if (target_dir.find('/lib') >=0):
           fs_dir = target_dir.replace(target_prefix, '')
           run_cmd('mkdir -p ' + 'fs/' + fs_dir)
           run_cmd('cp -d ' + i + ' fs/' + fs_dir)
        if (target_dir.find('/usr') >=0):
           fs_dir = target_dir.replace(target_prefix, '')
           run_cmd('mkdir -p ' + 'fs/' + fs_dir)
           run_cmd('cp  ' + i + ' fs/' + fs_dir)

    #cmd = 'cp -r ' + target_prefix + '/etc/rc.d/' + ' fs/etc/'
    #print cmd
    #run_cmd(cmd)

    cmd = ' cp ' + target_prefix  +  '/etc/{passwd,group,hosts} fs/etc '
    run_cmd(cmd)
    print cmd

    make_dev()

    cmd = ' chmod a+x fs/lib/lib* '
    run_cmd(cmd)
    cmd = ' chmod a+x fs/etc/* '
    run_cmd(cmd)

    print '\t====== coping additional libs ========'
    # libnss_* are required from login; but it's not possible get by ldd cmd
    cmd = 'cp  -d ' + target_prefix + '/lib/libnss*' + ' fs/lib/'
    run_cmd(cmd)
    print cmd
    cmd = 'cp  -d ' + target_prefix + '/lib/libnss_nis*' + ' fs/lib/'
    run_cmd(cmd)
    print cmd
    cmd = 'cp  -d ' + target_prefix + '/lib/libnss_nisplus*' + ' fs/lib/'
    run_cmd(cmd)
    print cmd

    # libgcc_s.so.1 is reqired from libpthread but it's not possible get it by ldd cmd
    for i in lib_list:
        if i.find('libpthread') >= 0:
           cmd = 'cp   -a ' + target_prefix + '/lib/libgcc_*' + ' fs/lib/'
           run_cmd(cmd)
           print cmd
    print '\t======================================='

    if init_type == 'busybox':
       setup_busybox()
    if init_type == 'sysv':
       setup_sysvinit()

#------------------------------------------------

def do_cpio(path):
    """
    """
    print 'doing fs.cpio \n'
    cmd = 'cd ' + str(path) + ' ; find . | cpio -ovB -H newc >  ../fs.cpio  '
    print cmd
    get_cmd_output(cmd)

#------------------------------------------------

def usage():
    print '\n\nDESCRIPTION:\nStarting from the installed binary RPM (for SH4), it discover '
    print 'the minimal set of shared library object needed from a dinamically linked application.'
    print 'It also returns, a filesystem skeleton, including a small set of selected binaries'
    print '\n  -h,  --help   Usage information.'
    print '\n  -b,  --binary <file> executable file; use " " to specify more than one bin '
    print '         (example: -b "gzip ls pwd") '
    print '\n  -t,  --target_prefix <path> the target path location '
    print '         (default: /opt/STM/STLinux-2.4/devkit/sh4/target/)'
    print '\n  -i   --init_type : '
    print '\t\t\t  busybox '
    print '\t\t\t  sysv '
    print '\t\t\t  no (no init files) '
    print 'example: ./do_min_fs.py -i busybox -t /opt/STM/STLinux-2.4/devkit/sh4/target -b "file more"'
    print '\n\n\n'
    sys.exit()

#--------------------------------------------------

def get_menu_opt(argv):
    """ print a menu and return a list with selected options
    """
    try:
#      opts = ''
#      args = ''
       opts , args = getopt.gnu_getopt(argv, 'hb:t:i:', ['--init_type', '--binary=', '--target_prefix=', '--help'])
    except getopt.GetoptError:
           usage()
    target_prefix = ''
    console = ''
    binary_list=[]
    for o, v  in opts:
       if o == '-b' or o == '--binary':
          v = v.split(' ')  # take out all blank spaces and replace the v string with  binary_list
          for i in v:
            if i != '':
               binary_list.append(i)
       elif o == '-t' or o == '--target_prefix':
            target_prefix = v
       elif o == '-i' or o == '--init_type':
            console = v
       elif o == '-h' or o == '--help':
          usage()
    params = []
    params.append(binary_list)
    params.append(console)
    params.append(target_prefix)
    return params

#-----------------------------------------

def get_library(command):
    """ input: the binary name
        output: a list of all libraries (and config files
        inside <target>/etc) used from 'command'.
    """
    cmd =  'find '+  target_prefix + ' -name ' + command
    resu = []
    resu = get_cmd_output_2(str(cmd))
    raw_paths = resu[0].splitlines()

    # get only the binary command: remove all paths that not include 'bin' (or 'sbin')
    paths = []
    for j in raw_paths:
        #if j.find('bin') >=0 :
        if (j.find('/target/bin') >=0 or j.find('/target/sbin') >=0 or j.find('/usr/bin') >=0 \
            or j.find('/usr/local/bin') >=0  or j.find('/usr/sbin') >=0 \
            or j.find('/usr/local/sbin') >=0 ):
           paths.append(j)
    # for a given bin path, get the package name (if it exist)
    rpm_package_name = ' '
    print '\npaths: ' + str(paths)
    for i in paths:
        print 'rpm -qf ' + str(i)
        pkg = get_cmd_output('rpm -qf ' + i)
        if ((pkg[0].find('is not owned') == -1  and  pkg[0].find('such file') == -1)):
           rpm_package_name =  pkg
           binary_command = i
    raw_list=[]
    if (rpm_package_name == " "):
       print 30*'=' + '\n Warning: ' + str(command) + ' Package not found \n' + 30*'='
       return raw_list

    print '\n  binary_command: ' + str(binary_command)
    print '  rpm_package_name: ' + str(rpm_package_name)
    # we want to copy into the minimal FS the binary too
    raw_list.append(binary_command)

    line=[]
    line = get_lib_path(run_cmd(ldd_cmd_sh4 + ' ' + binary_command))
    for i in line:
        raw_list.append(i)

    # now, get the file list from the rpm pkg and search for config files under /etc and /lib
    rpm_output = get_cmd_output('rpm -qli ' + rpm_package_name[0])
    rpm_path=[]
    # extract paths from the rpm output and put it into the rpm_path[] list
    for i in range(len(rpm_output)):
        if rpm_output[i][0:1] == '/':
            rpm_path.append(rpm_output[i])
    del rpm_output
    #print 'rpm_path ' + str(rpm_path)

    # analyze the RPM pkg file list and get scripts and lib files
    # shared library are got by ldd cmd in get_library()
    for i in rpm_path:
        cmd_file_result = run_cmd(' file ' + i)
        resu = str(cmd_file_result[0])

        if (i.find("bin") >= 0):
           if (resu.find('ASCII') >=0 or resu.find('script text') >=0 ):
              print '        adding: ' + str(i)
              raw_list.append(i)

        if (i.find("/etc/") >= 0):
           if (resu.find('ASCII') >=0 or resu.find('script text') >=0 or resu.find('data') >= 0 \
           or resu.find('text') >= 0):
              print '        adding: ' + str(i)
              raw_list.append(i)

        if (i.find("/lib") >= 0):
           if (resu.find('ASCII') >=0 or resu.find('script text') >=0):
              print '        adding: ' + str(i)
              raw_list.append(i)
           if (resu.find("ELF 32") >= 0 or resu.find("symbolic link to") >= 0):
              print '        adding: ' + str(i)
              raw_list.append(i)

            # take out docs man README info
        if (i.find("/doc/") == -1  and  i.find('/man/') == -1 and i.find('READ') == -1 \
        and i.find('info') == -1):
           if (i.find("/share/") >= 0):  # look inside /share dir
              if (resu.find('ASCII') >=0 or resu.find('script text') >=0 \
              or  resu.find('magic') >=0 or resu.find('data') >= 0):
                 print '       found /share file adding: ' + str(i)
                 raw_list.append(i)

    return unique(raw_list)

#--------------------------------------------------

def get_common_path(s1, s2):
    com = ''
    for i in range(len(s1)):
        if s1[i] == s2[i]:
           com = str(com) + str(s1[i])
        else:
           break
    com = str(com) + '*'
    return com


## ================================================================
## 		       		Main
## =================================================================

#os.environ['LDD_ROOT_SHOW'] = '0'

global target_prefix
target_prefix = '/opt/STM/STLinux-2.4/devkit/sh4/target'
boot_type = 'busybox' # default
user_param = ['', '', '']
user_param = get_menu_opt(sys.argv[1:])
bin_list = user_param[0]  # command list to find

if user_param[1] != '':
   boot_type = user_param[1] # default busybox
if user_param[2] != '':
   target_prefix =  user_param[2]

print 30*'='
print '  bin: ' + str(bin_list)
print '  boot_type: ' + str(boot_type)
print '  target_prefix:  ' + str(target_prefix)
print 30*'='

ldd_cmd_sh4 = target_prefix + '/../../../host/bin/ldd'
os.environ['LDD_ROOT_BASE'] = target_prefix
line=[]
raw_library_list=[]

#  minimal command set, for bash.
#  killall5 poweroff shutdown telinit
bin_4_bash = ['bash', 'login', 'init', 'grep', 'uname', 'hostname', 'readlink', 'cat',\
'mount', 'getty', 'agetty', 'stty', 'ls', 'rm', 'pwd', 'mountpoint', 'id', 'fsck',\
'mknod', 'halt', 'chmod', 'runlevel']

# setup libs for bash
if boot_type != 'no':
   if boot_type == 'sysv':
      for i in bin_4_bash:
          line = get_library(i)
          for k in line:
              raw_library_list.append(k)
   if boot_type == 'busybox':
      line = get_library('busybox')
      for j in line:
          raw_library_list.append(j)

for i in range(len(bin_list)):
    line =  get_library(bin_list[i])
    for j in line:
        raw_library_list.append(j)

library_list_swp = unique(raw_library_list)
library_list = del_line_feed(library_list_swp)
del library_list_swp

file_list = []
for i in library_list:
    cmd = 'readlink ' + str(i)
    file = get_cmd_output(cmd)
    if file != [] and (i.find('/lib/') >= 0):
       file_path = os.path.dirname(i) +'/' + str(file[0])
       common = get_common_path(str(i), str(file_path))
       file_list.append(str(common))
    else:
       file_list.append(str(i))

library_list = file_list

print '\n  ======== libs bin and config files ========='
for j in library_list:
    print j
print '     ' + 30*'='  + '\n'

gen_fs(library_list, boot_type)
do_cpio('fs')
