#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <signal.h>

static inline void get_path(pid_t pid, char *exe, size_t maxLen)
{
    char proc[64];

    snprintf(proc, sizeof(proc), "/proc/%d/exe", pid);
    int err = readlink(proc, exe, maxLen - 1);
    exe[err > 0 ? err : 0] = '\0';
    LOGD("brcm_bt_helper:get_path: pid:%d, proc:%s, readline ret:%d, exe:%s", pid, proc, err, exe);
}

/*******************************************************************************
**
** Function         find_active_net_interface
**
** Description     Automatically discover active network interface 
**
**
** Returns          void
*******************************************************************************/

#define PPPDCMD "pppd"
#ifndef TRUE
#define TRUE 1
#define FALSE 0
typedef int BOOLEAN;
#endif
#define TTY_NAME_DUN "/dev/ttySA0"
int net_iface_active(const char *name, int ifc_ctl_sock)
{    
    unsigned int addr, mask, flags;    
    struct ifreq ifr;

    memset((void*)&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, name, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;

    if (ioctl(ifc_ctl_sock, SIOCGIFADDR, &ifr) < 0)
    {
        addr = 0;
    }
    else
    {
        addr = ((struct sockaddr_in*) &ifr.ifr_addr)->sin_addr.s_addr;
    }

    if (ioctl(ifc_ctl_sock, SIOCGIFFLAGS, &ifr) < 0)
    {
        flags = 0;
    }
    else
    {
        flags = ifr.ifr_flags;
    }


    if (flags & 1)
        return TRUE;

    return FALSE;
}


static int get_active_interface(char *netname)
{
    DIR *d;
    struct dirent *de;
    BOOLEAN retval = FALSE;

    int ifc_ctl_sock = socket(AF_INET, SOCK_DGRAM, 0);    

    d = opendir("/sys/class/net");
    if (d == 0) 
    {
        close(ifc_ctl_sock);
        return -1;
    }

    while ((de = readdir(d)))
    {

        if (de->d_name[0] == '.') continue;

        /* skip local interface */
        if (strncmp(de->d_name, "lo", 2)==0) continue;

        /* assume only one active interface */

        /* use property to specify name of desired network interface ? */

        if (net_iface_active(de->d_name, ifc_ctl_sock) == TRUE)
        {
            sprintf(netname, "%s", de->d_name);
            LOGD("Found active network interface [%s]", netname);
            retval = TRUE;
            break;
        }
    }

    closedir(d);
    if (ifc_ctl_sock > 0) {
        close(ifc_ctl_sock);
    }
    return retval;
}
static void nat_setup(char *netname)
{
    char cmdline[255];

    LOGD("Setting up private network for remote peer net if [%s]", netname);

    // fixme -- setup more rules using separate script

    /* setup iptables for NAT */       
    sprintf(cmdline, "iptables -t nat -A POSTROUTING -o %s -j MASQUERADE", netname);
    LOGD("%s", cmdline);
    system(cmdline);

    /* make sure ip forwarding is enabled */
    sprintf(cmdline, "echo 1 > /proc/sys/net/ipv4/ip_forward");
    LOGD("%s", cmdline);            
    system(cmdline);
}
void pppd_main(const char *ip_addresses, const char* btlif_socket)
{
    char netname[100];    
    char ms_dns[40];
    int result;
    unsigned char *pppd_options[16];
    if (get_active_interface(netname) == TRUE)
    {
        /* setup iptables for nat */
        nat_setup(netname);  
    }
    else
    {
        /* no active network interface found, skip iptables */ 
        LOGD("No active network interface found, skip nat setup");
    }

    property_get("net.dns1", ms_dns, "0.0.0.0");

    int i = 0;
    pppd_options[i++] = PPPDCMD; 
    if(btlif_socket)
    {
        pppd_options[i++] = "socket";
        pppd_options[i++] = btlif_socket;
    }
    else
    {
        pppd_options[i++] = TTY_NAME_DUN;
        pppd_options[i++] = "115200"; /* dummy value */
        pppd_options[i++] = "crtscts"; 
        pppd_options[i++] = "local";
    }
    pppd_options[i++] = ip_addresses;
    pppd_options[i++] = "debug";
    pppd_options[i++] = "noauth";
    pppd_options[i++] = "nopersist";
    pppd_options[i++] = "nodetach";    /* needed to be able to kill pppd process upon DG exit    */
    //pppd_options[i++] = "silent";    /* pts requires server to initiate LCP negotiation */
    pppd_options[i++] = "passive";
    pppd_options[i++] = "proxyarp";    /* 'send all packets to me' */
    pppd_options[i++] = "ktune";       /* enables ip_forwarding */
    pppd_options[i++] = "ms-dns";
    pppd_options[i++] = ms_dns;
    pppd_options[i++] = NULL;

#if 1
    {
        /* print pppd_options */
        i = 0;
        while (pppd_options[i])
        {
            LOGD("brcm_bt_helper: pppd option[%d]:%s", i, pppd_options[i]);
            i++;
        }
    }
#endif

    result =  execvp(PPPDCMD, pppd_options);

    if (result < 0)
    {
        LOGE("brcm_bt_helper exiting pppd (%s)", strerror(errno));
    }
}
static pid_t pppd_pid = -1;
static void pppd_signal_handler(int n)
{
    LOGD("brcm_bt_helper: pppd_signal_handler pppd_pid:%d", pppd_pid);
    if(pppd_pid > 0)
       kill(pppd_pid, SIGTERM);
}
static int cmd_pppd(int argc, char*argv[])
{
    //argv[0]: ip_address; argv[1]: stocket_address

    LOGD("brcm_bt_helper cmd pppd: argc:%d, arg[0]:%s, arg[1]:%s", argc, argv[0], argv[1]);
    if(argc < 1)
    {
        LOGE("brcm_bt_helper cmd pppd argc:%d, not enough arguments", argc);
        return -1;
    }
    sigset_t block_term;
    /* Initialize the signal mask. */
    sigemptyset (&block_term);
    sigaddset (&block_term, SIGTERM);
    sigprocmask (SIG_UNBLOCK, &block_term, NULL);

    if(signal(SIGTERM, pppd_signal_handler) == SIG_ERR)
        LOGE("signal SIGTERM set handler error");
    else 
        LOGD("signal SIGTERM set handler succeeded");
       /* now fork off a pppd process */
    pid_t pid = fork();


    if (pid < 0)
    {
        LOGE("brcm_bt_helper fork()");
        return -1;
    }
    else if (pid == 0)
    {
        /* now start actual pppd */
        pppd_main(argv[0], argv[1]);
        _exit(0);
    }
    pppd_pid = pid;
    LOGD("brcm_bt_helper: wait for pppd pid: %d exit", pid);
    int status;
    waitpid(pid, &status, 0);
    LOGD("brcm_bt_helper: wait for pppd pid: %d returned", pid);
    return 0;
}

#if 0
#include <private/android_filesystem_config.h>
#define contactdb "/data/data/com.android.providers.contacts/databases/contacts2.db"
static int cmd_contact(int argc, char* argv[])
{
    LOGD("brcm_bt_helper cmd contact: argc:%d, arg[0]:%s", argc, argv[0]);
    if(argc < 1)
    {
        LOGE("brcm_bt_helper cmd contact argc:%d, not enough arguments", argc);
        return -1;
    }
    if(strcmp(argv[0], "on") == 0)
        return chown(contactdb, -1, AID_BLUETOOTH);
    if(strcmp(argv[0], "off") == 0)
    {
        struct stat fstate = {0};
        int ret  = stat(contactdb, &fstate);
        if(fstate.st_gid == AID_BLUETOOTH)
            return chown(contactdb, -1, fstate.st_uid);
    }
    return -1;
}
#endif
#define btld "/system/bin/btld"
int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        LOGE("%s: not enough arguments", argv[0]);
        return -1;
    }
    char exe[64] = {0};
    get_path(getppid(), exe, sizeof(exe));
    if(strcmp(exe, btld))
    {
        LOGE("%s is not authorized to launch %s", exe, argv[0]);
        return -1;
    }
#if 1
    {   
        int i;
        for(i = 0; i < argc; i++)
            LOGD("brcm_bt_helper: argc:%d, argv[%d]:%s", argc, i, argv[i]);
    }
#endif

    
    char* cmd = argv[1];
    char** cmd_argv = argv + 2;
    int cmd_argc = argc - 2;
    //process cmd
    if(strcmp(cmd, "pppd") == 0)
        return cmd_pppd(cmd_argc, cmd_argv);
    //if(strcmp(cmd, "contact") == 0)
    //    return cmd_contact(cmd_argc, cmd_argv);
    LOGE("%s:unknown argument:%s", argv[0], argv[1]);
    return -1;        
}

