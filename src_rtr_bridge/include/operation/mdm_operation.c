#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <mosquitto.h>

#include <basic/basic.h>
#include <operation/mdm_operation.h>
#include "mqtt.h"

int read_active_simVal(void)
{
    char gpio_pth[] = {"/sys/class/gpio/gpio22/value"};
    int fd2 = open(gpio_pth,O_RDONLY);
    int ret = 0;
    char str[2] = {0};
    if (fd2<0)
    {
        dbg_print(Bold_Red,\
            "ERR : open() failed in read_active_sim() due to : %s\n",\
            strerror(errno));
        return -1;
    }
    ret = read(fd2,str,1);
    if( ret < 0 ) 
    {
        close(fd2);
        return -1;
    }
    if(strlen(str)>0)
        ret = atoi(str);
    close(fd2);
    return ret;
}

int read_active_sim(void)
{
    char sim_pth[] = {"/reap/etc/config/ActiveSIMStatus"};
    char gpio_pth[] = {"/sys/class/gpio/gpio22/value"};
    char str[2]= {0}, str2[2] = {0};
    int ret, ret2;
    int fd = -1, fd2;
    // sprintf(sim_pth,"/reap/etc/config/ActiveSIMStatus");
    fd = open(sim_pth,O_RDONLY);
    fd2 = open(gpio_pth,O_RDONLY);
    if( (fd<0) && (fd2<0) )
    {
        dbg_print(Bold_Red,\
            "ERR : open() failed in read_active_sim() due to : %s\n",\
            strerror(errno));
    }
    ret = read(fd,str,1);
    ret2 = read(fd2,str2,1);
    if( (ret < 0) || (ret2 < 0) )
    {
        close(fd);
        close(fd2);
        return -1;
    }
    ret = atoi(str);
    ret2 = atoi(str2);
    ret2 += 1;
    close(fd);
    close(fd2);
    // if(ret != ret2)
    // {
    //     ret = -1;
    // }
    return ret;
}


int mdm_selSim_ndt(stMSQ_DS_ *spMqtt_ds, uint32_t sim)
{
    int ret = 0;
    int fd,fd2;
    char caPth[] = {"/sys/class/gpio/gpio22/value"};
    char sim_pth[] = {"/reap/etc/config/ActiveSIMStatus"};
    char cVal[2] = {0};
    int Asim = read_active_simVal();
    if(Asim < 0)
    {
        return Asim;
    }
    Asim += 1; // User input is 1 / 2
    if(Asim == sim)
    {
        dbg_print(Bold_Cyan,"\nsim=%d is already active.\n",sim);
        return Asim;
    }
    
    if( (sim < 1) && (sim > 2) )
    {
        dbg_print(Bold_Red,"\nERR : Invalid sim-%d\n",sim);
        ret = -1;
    }
    else
    {
        dbg_print(NULL,"Selecting sim-%d\n",sim);
        fd = open(caPth,O_WRONLY);
        fd2 = open(sim_pth,O_WRONLY);
        if(fd < 0 )
        {
            dbg_print(Bold_Red,"ERR-mdm_selSim_ndt : open(gpio22) failed due to : %s\n",strerror(errno));
            dbg_print(Bold_Red,"ERR : Returning from mdm_selSim_ndt()\n");
            wr_klog("ERR-rtr-brz : mdm_selSim_ndt(), open(gpio22) failed.\n");
            close(fd);
            close(fd2);
            return -1;
        }
        if(fd2<0)
        {
            dbg_print(Bold_Red,"ERR-mdm_selSim_ndt : open(ActiveSIMStatus) failed due to : %s\n",strerror(errno));
            dbg_print(Bold_Red,"ERR : Returning from mdm_selSim_ndt()\n");
            wr_klog("ERR-rtr-brz : mdm_selSim_ndt(), open(ActiveSIMStatus) failed.\n");
            close(fd);
            close(fd2);
            return -1;   
        }
        ret = mosquitto_publish(spMqtt_ds->stpMsq_instns, NULL, \
            TPQ_SIMPWR_RQST,strlen(TPQ_SIMPWR_MSGof),TPQ_SIMPWR_MSGof,1,false);
        if(ret != MOSQ_ERR_SUCCESS)
        {
            mqtt_printErr(ret,"ERR-MQTT : in mdm_selSim_ndt(), mosquitto_publish() failed due to : ");
            dbg_print(Bold_Red,"ERR : Returning from mdm_selSim_ndt()...\n");
            close(fd);
            close(fd2);
            return -1;
        }
        sleep(4);
        msleep(500);
        sprintf(cVal,"%d",sim-1);
        ret = write(fd,cVal,1);
        if( ret < 0 )
        {
            dbg_print(Bold_Red,"ERR-mdm_selSim_ndt : write() failed due to : %s\n",strerror(errno));
            dbg_print(Bold_Red,"ERR-mdm_selSim_ndt : Returning... .. .\n");
            close(fd);
            close(fd2);
            return -1;
        }

        sleep(4);
        ret = mosquitto_publish(spMqtt_ds->stpMsq_instns, NULL, \
            TPQ_SIMPWR_RQST,strlen(TPQ_SIMPWR_MSGon),TPQ_SIMPWR_MSGon,1,false);
        if(ret != MOSQ_ERR_SUCCESS)
        {
            mqtt_printErr(ret,"ERR-MQTT : in mdm_selSim_ndt(), mosquitto_publish() failed due to : ");
            dbg_print(Bold_Red,"ERR : Returning from mdm_selSim_ndt()...\n");
            close(fd);
            close(fd2);
            return -1;
        }
        sleep(4);
        memset(cVal,0,2);
        sprintf(cVal,"%d",sim);
        ret = write(fd2,cVal,1);
        if( ret < 0 )
        {
            dbg_print(Bold_Red,"ERR-mdm_selSim_ndt : write() failed due to : %s\n",strerror(errno));
            dbg_print(Bold_Red,"ERR-mdm_selSim_ndt : Returning... .. .\n");
            close(fd);
            close(fd2);
            return -1;
        }
        ret = sim;
        dbg_print(Bold_Yellow,"Time taken to switch sim-%d : %dSec",sim,12);
    }
    close(fd);
    close(fd2);
    return ret;
}

int mdm_selSim(uint32_t sim, uint32_t *tm_tkn)
{
    FILE *fp_rsp = NULL;
    FILE *fp_snd = NULL;
    int ret = 0;
    char mos_cmd[120]={0};
    // char buf[1024] = {0};
    int Dtm = RTR_SIM_SWCH_TM; //Maximum delay time 
    int Asim = read_active_sim();
    int sts = 0;
    if(Asim < 0)
    {
        return Asim;
    }
    else if(Asim == sim)
    {
        dbg_print(Bold_Cyan,"\nsim=%d is already active.\n",sim);
        return Asim;
    }
    if( (sim < 1) && (sim > 2) )
    {
        dbg_print(Bold_Red,"\nERR : Invalid sim-%d\n",sim);
        ret = -1;
    }
    else
    {
        sprintf(mos_cmd,"mosquitto_pub -d -t \"Isensev2/SimSwitch/CommandRequest\" -m \"<,1,SELSIM%d,\\\"2020-09-21 11:00:00\\\",>\"",sim);
        fp_snd = popen(mos_cmd,"r");
        if(fp_snd == NULL)
        {
            perror("\nmosquitto_pub() failed due to : ");
            ret = -1;
        }
        else
        {
            printf("\nSelecting SIM%d. Mnimum processing time : %dSec.\n",sim,Dtm);
            do
            {
                Asim = read_active_sim();
                sleep(1);
            } while ( (Asim!=sim) && (sts++ <= Dtm) );
            if(Asim != sim)
            {
                printf("Could not switch the sim. Time spent = %d\n",sts);
            }
            else
            {
                printf("Time taken to switch = %d\n",sts);
            }
            pclose(fp_snd);
            ret = Asim;
            *tm_tkn = sts;
        }
    }
    return ret;
}



/***********************************************
 * 
 * mdm_init() : open "/dev/ttyAT" and set a 
 * proper termios struct.
 * 
 * ARG : termios struct pointer
 * RET : {+ve file descriptor on success;
 *          -1 on error}
 * *******************************************/
int mdm_init(Termios *tty)
{
	memset (tty, 0, sizeof *tty);
    int ttyFD, tm_out=60;
    do
    {
        ttyFD = open("/dev/ttyAT", O_RDWR | O_NONBLOCK | O_NDELAY );
        if(ttyFD < 0)
        {
            sleep(1);
        }
    } while ( (ttyFD < 0) && (tm_out-- > 0));
    
    if(ttyFD < 0 )
    {
        perror("open() failed due to :");
        printf("Exiting... .. .\n");
        close(ttyFD);
        return ttyFD;
    }
    
	if (tcgetattr ( ttyFD, tty ) != 0 )
    {
        perror("tcgetattr() failed due to :");
        printf("Exiting... .. .\n");
        close(ttyFD);
        return -1;
    }
    cfsetospeed (tty, B115200);
    cfsetispeed (tty, B115200);
    tty->c_cflag     &=  ~PARENB;                            // parity 
    tty->c_cflag     &=  ~CSTOPB;                            // stop bits 
    //tty.c_cflag   &=  ~CSIZE;                             // 
    tty->c_cflag     |=  CS8;                                // data bits 
    tty->c_cflag     &=  ~CRTSCTS;                           // no hardware flow control 
    tty->c_iflag     &=  ~(IXON | IXOFF | IXANY);            // no s/w flow ctrl 
    tty->c_lflag     =   0;                                  // non canonical 
    tty->c_oflag     =   0;                                  // no remapping, no delays 
    tty->c_cc[VMIN]  =   0;                                  // read doesn't block 
    tty->c_cc[VTIME] =   0;                                  // read timeout 
    tty->c_cflag     |=  CREAD | CLOCAL;                     // turn on READ & ignore ctrl lines 
    tty->c_lflag     &=  ~(ICANON | ECHO | ECHOE | ISIG);    
    //tty.c_oflag     &=  ~OPOST;                           
    
    tcflush( ttyFD, TCIFLUSH );
    if ( tcsetattr ( ttyFD, TCSANOW, tty ) != 0)
    {
        perror("tcsetattr() failed due to :");
        printf("Exinting... .. .\n\r");
        close(ttyFD);
        return -1;
    }
    write(ttyFD,RTR_TTY_SET,sizeof(RTR_TTY_SET));
    msleep(50);
    write(ttyFD,RTR_KSREP,sizeof(RTR_KSREP));
    msleep(100);
    fd_ctrl(ttyFD,1,tty);
    return ttyFD;
}

/***************************************************
 * Arg : 
 *  1 (clean the fd and open for read/write)
 *  0 (Turn off the file descripter for read/write)
 * Ret :
 *  Success :
 *      1 : Active. open for R/W
 *      0 : Sleep. Can not R/W
 *  Error : 
 *      -1
 **************************************************/
int fd_ctrl(int fd, int ctrl, Termios *tty)
{

    int ret = 0;
    int i=1;
    char buf[100] = {0};
    if (ctrl == 1)
    {
        tcflush( fd, TCIOFLUSH);
        tcflow(fd,TCION);
        tcflow(fd,TCOON);
        do
        {
            ret = read(fd,buf,100);
            msleep(1);
            dbg_print(NULL,"Cleaning ttyfd(%d); ret = %d\n",i,ret);
        } while ( (ret > 0) && (i <= 5));
        ret = 1;
    }
    else if(ctrl == 0)
    {
        tcflow(fd,TCIOFF);
        tcflow(fd,TCOOFF);
        ret = 0;
    }
    else if(ctrl == 2)
    {
        do
        {
            ret = read(fd,buf,100);
            if(ret >0)
                msleep(5);
            // memset(buf,0,strlen(buf));
        } while ( (ret > 0) && (i++ <= 5));
        ret = 1;
        return ret;
    }
    
    if ( tcsetattr (fd, TCSANOW, tty ) != 0)
    {
        perror("\n\rIN fd_ctrl(), tcsetattr() failed due to :");
        ret = -1;
    }
    return ret;
}


/*****************************************
 * mdm_get_model() : get manufacturer model
 * return : success = 0
 *          error = -1
 * *************************************/
int mdm_get_model(int fd, char *mdl)
{
    char rd_str[700];
    int tm = 0;
    fd_ctrl(fd,2,NULL);
    fd_ctrl(fd,2,NULL);
    int rt_rdwr = write(fd,RTR_MDL,strlen(RTR_MDL));
    int sts = 0;
    int ret = 0;

    if (rt_rdwr > 0)
    {
        do
        {
            tm += 1;
            rt_rdwr = read(fd,rd_str,50);
            if( strstr(rd_str,"OK\r\n") )
            {
                sts = 1;
                break;
            }
            msleep(100);
        } while( tm<=10 );
        if(sts == 1)
        {
            // printf("\n----success----\n");
            char *lns[5] = {0};
            sts = split_line(rd_str,lns,5);
            // printf("split_line() = %d\n",sts);
            if(sts > 2)
            {
                memset(mdl,0,strlen(mdl));
                strcpy(mdl,lns[1]);
                rmv_nlcr(mdl);
                ret = 0;
            }
            else
            {
                printf("\nERR-GET_MDL : line(2) of modem response does not have mdl info\n");
                ret = -1;
            }            
        }   
        else
        {
            printf("\n-----error in mdm_get_model()-----\n");
            ret -1;
        }
        
    }
    else
    {
        ret = -1;
    }
    return ret;
}

/*****************************************
 * mdm_get_mfn() : get manufacturer name
 * return : success = 0
 *          error = -1
 * *************************************/
int mdm_get_mfn(int fd, char *mfn)
{
    char rd_str[700]={0};
    int tm = 0;
    fd_ctrl(fd,2,NULL);
    fd_ctrl(fd,2,NULL);
    int rt_rdwr = write(fd,RTR_MFN,strlen(RTR_MFN));
    int sts = 0;
    int ret = 0;

    if (rt_rdwr > 0)
    {
        do
        {
            
            rt_rdwr = read(fd,rd_str+(strlen(rd_str)),70);
            if( strstr(rd_str,"OK\r\n") )
            {
                sts = 1;  
                // printf("strlen(rd_str) = %d\n",strlen(rd_str));
                break;
            }
            msleep(100);
        } while ((tm++) <= 10);
        if(sts == 1)
        {
            char *lns[5] = {0};
            sts = split_line(rd_str,lns,5);
            if(sts > 2)
            {
                memset(mfn,0,strlen(mfn));
                // for(int i=0;i<sts;i++)
                // {
                //     printf("l%d : %s\n",i,lns[i]);
                // }
                strcpy(mfn,lns[1]);
                rmv_nlcr(mfn);
                ret = 0;
            }
            else
            {
                printf("\nERR-GET_MFN : line(2) of modem response does not have mfn info\n");
                ret = -1;
            }            
        }
        else
        {
            printf("\n-----error in mdm_get_mfn()-----\n");
            ret = -1;
        }
    }
    else
    {
        ret = -1;
    }
    return ret;
}

int mdm_get_sn(int fd, char *sn)
{
    char rd_str[800]={0};
    int tm = 0;
    fd_ctrl(fd,2,NULL);
    fd_ctrl(fd,2,NULL);
    int rt_rdwr = write(fd,RTR_SN,strlen(RTR_SN));
    int sts = 0;
    int ret = 0;
    char *rptr = NULL; 

    if (rt_rdwr > 0)
    {
        do
        {
            msleep(100);
            rt_rdwr = read(fd,rd_str+(strlen(rd_str)),100);
            if( strstr(rd_str,"OK\r\n"))
            {
                sts = 1;  
                break;
            }
            // memset(rd_str,0,700);            
        } while ((tm++) <= 6);
        if(sts == 1)
        {
            char *lns[15] = {0};
            int i=0;
            rptr = strstr(rd_str,"AT+GSN");
            if(rptr)
            {
                sts = split_line(rd_str,lns,5);
                for(i=0;i<sts;i++)
                {
                    if(strstr(lns[i],"GSN"))
                    {
                        i += 1;
                        rmv_nlcr(lns[i]);
                        break;
                    }
                    else if(is_numeric(lns[i])>0)
                    {
                        break;
                    }
                }
                
                if(is_numeric(lns[i])>0)
                {
                    memset(sn,0,strlen(sn));
                    rmv_nlcr(lns[i]);
                    strcpy(sn,lns[i]);                
                    ret = 0;
                }
                else
                {
                    dbg_print(Bold_Red,"\nERR-GET_MFN : line(2) of modem response does not have sn info\n");
                    ret = -1;
                }   
            }         
        }
        else
        {
            dbg_print(Bold_Red,"\n-----error in mdm_get_sn()-----\n");
            ret = -1;
        }
    }
    else
    {
        ret = -1;
    }
    return ret;

}

/**********************************
 * Arguments :
 *  fd : {file descriptor}
 *  Return :
 *      success = 0
 *      Error   = -1
 * *******************************/
int mdm_get_fmwv(int fd, char *fmwv)
{
    char rd_str[700]={0};
    int tm = 0;
    fd_ctrl(fd,2,NULL);
    fd_ctrl(fd,2,NULL);
    int rt_rdwr = write(fd,RTR_FWV,strlen(RTR_FWV));
    int sts = 0;
    int ret = 0;

    
    if (rt_rdwr > 0)
    {
        do
        {
            msleep(100);
            rt_rdwr = read(fd,rd_str+(strlen(rd_str)),70);
            if( strstr(rd_str,"OK\r\n") )
            {
                sts = 1;  
                // printf("strlen(rd_str) = %d\n",strlen(rd_str));
                // printf("rd_str : %s\n",rd_str);
                break;
            }
            
        } while ((tm++) <= 10);

        char *fnm = {"mdm_get_fmwv"};
        char del_str[900] = {0};
        sprintf(del_str,"%s : %s : %s\n",KLG_NM,fnm,rd_str);
        wr_klog(del_str);
        memset(del_str,0,strlen(del_str));


        if(sts == 1)
        {
            char *lns[5] = {0};
            sts = split_line(rd_str,lns,5);
            wr_klog("RTR-BRZ : mdm_get_fmwv : ");
            if(sts > 0)
            {
                memset(fmwv,0,strlen(fmwv));
                uint32_t i1=0;
                for(i1=0;i1<sts;i1++)
                {
                    if(strstr(lns[i1],"jenkins"))
                    {
                        sts = 111;
                        break;
                    }
                }
                if(sts==111)
                {
                    strcpy(fmwv,lns[i1]);
                    rmv_nlcr(fmwv);
                    ret = 0;

                    sprintf(del_str,"DBG-RTR-BRZ : fmwv = %s\n",fmwv);
                    wr_klog(del_str);
                }
                else
                {
                    ret = -1;
                    printf("RTR-BRZ : mdm_get_fmwv : Error\n");
                    wr_klog("RTR-BRZ : mdm_get_fmwv : Error\n");
                }
            }
            else
            {
                printf("\nERR-GET_FWV : line(2) of modem response does not have fmwv info\n");
                wr_klog("ERR-RTR-BRZ : fmwv info is missing in mdm_rsp.\n");
                ret = -1;
            }            
        }
        else
        {
            printf("\n-----error in mdm_get_fmwv()-----\n");
            wr_klog("ERR-RTR-BRZ : mdm_get_fmwv() failed.\n");
            ret = -1;
        }
    }
    else
    {
        ret = -1;
    }
    return ret;
}

int mdm_get_csq(int fd, char *csq)
{
    char rd_str[700]={0};
    int tm = 0;
    fd_ctrl(fd,2,NULL);
    fd_ctrl(fd,2,NULL);
    int rt_rdwr = write(fd,RTR_CSQ,strlen(RTR_CSQ));
    int sts = 0;
    int ret = 0;

    if (rt_rdwr > 0)
    {
        do
        {
            msleep(100);
            rt_rdwr = read(fd,rd_str+(strlen(rd_str)),70);
            if( strstr(rd_str,"OK\r\n") )
            {
                sts = 1;  
                // printf("strlen(rd_str) = %d\n",strlen(rd_str));
                // printf("rd_str : %s\n",rd_str);
                break;
            }
            
        } while ((tm++) <= 10);
        if(sts == 1)
        {
            char *lns[5] = {0};
            sts = split_line(rd_str,lns,5);
            int i;
            for(i=0;i<sts;i++)
            {
                if( strstr(lns[i],"CSQ:") )
                {
                    sts = 0;
                    break;
                }
            }
            if(sts == 0)
            {
                memset(csq,0,strlen(csq));
                // for(int i=0;i<sts;i++)
                // {
                //     printf("l%d : %s\n",i,lns[i]);
                // }
                char *tmp = strchr(lns[i],':');
                uint32_t sz_tmp = strlen(tmp);
                for(int i=0;i<sz_tmp;i++)
                {
                    if( ((tmp[i] >= '0') && (tmp[i] <= '9')) || (tmp[i] == '-') )
                    {
                        tmp += i;
                        break;
                    }
                }
                strcpy(csq,tmp);
                rmv_nlcr(csq);
                ret = 0;
            }
            else
            {
                printf("\nERR-GET_CSQ : line(2) of modem response does not have csq info\n");
                ret = -1;
            }            
        }
        else
        {
            printf("\n-----error in mdm_get_csq()-----\n");
            ret = -1;
        }
    }
    else
    {
        ret = -1;
    }
    return ret;
}




int mdm_get_paTmp(int fd, char *paTmp)
{
    char rd_str[700]={0};
    int tm = 0;
    fd_ctrl(fd,2,NULL);
    fd_ctrl(fd,2,NULL);
    int rt_rdwr = write(fd,RTR_PA_TMP,strlen(RTR_PA_TMP));
    int sts = 0;
    int ret = 0;

    if (rt_rdwr > 0)
    {
        do
        {
            msleep(100);
            rt_rdwr = read(fd,rd_str+(strlen(rd_str)),70);
            if( strstr(rd_str,"OK\r\n") )
            {
                sts = 1;  
                // printf("strlen(rd_str) = %d\n",strlen(rd_str));
                // printf("rd_str : %s\n",rd_str);
                break;
            }
            
        } while ((tm++) <= 10);
        if(sts == 1)
        {
            char *lns[5] = {0};
            sts = split_line(rd_str,lns,5);
            if(sts > 2)
            {
                memset(paTmp,0,strlen(paTmp));
                // for(int i=0;i<sts;i++)
                // {
                //     printf("l%d : %s\n",i,lns[i]);
                // }
                char *num[3]={0};
                sts = str2numstr(lns[2],num,5,0);
                // printf("temp : sts = %d \n",sts);
                if(sts>2)
                {
                    strcpy(paTmp,num[1]);
                    strcat(paTmp,".");
                    strcat(paTmp,num[2]);
                    ret =0;
                }
                else
                {
                    ret = -1;
                }
                
            }
            else
            {
                printf("\nERR-GET_PAT : line(2) of modem response does not have paTmp info\n");
                ret = -1;
            }            
        }
        else
        {
            printf("\n-----error in mdm_get_paTmp()-----\n");
            ret = -1;
        }
    }
    else
    {
        ret = -1;
    }
    return ret;

}



int mdm_get_pcTmp(int fd, char *pcTmp)
{
    char rd_str[700]={0};
    int tm = 0;
    fd_ctrl(fd,2,NULL);
    fd_ctrl(fd,2,NULL);
    int rt_rdwr = write(fd,RTR_PC_TMP,strlen(RTR_PC_TMP));
    int sts = 0;
    int ret = 0;

    if (rt_rdwr > 0)
    {
        do
        {
            msleep(100);
            rt_rdwr = read(fd,rd_str+(strlen(rd_str)),70);
            if( strstr(rd_str,"OK\r\n") )
            {
                sts = 1;  
                // printf("strlen(rd_str) = %d\n",strlen(rd_str));
                // printf("rd_str : %s\n",rd_str);
                break;
            }
            
        } while ((tm++) < 10);
        if(sts == 1)
        {
            char *lns[5] = {0};
            sts = split_line(rd_str,lns,5);
            if(sts > 2)
            {
                memset(pcTmp,0,strlen(pcTmp));
                // for(int i=0;i<sts;i++)
                // {
                //     printf("l%d : %s\n",i,lns[i]);
                // }
                char *num[3]={0};
                sts = str2numstr(lns[2],num,5,0);
                // printf("temp : sts = %d \n",sts);
                if(sts==2)
                {
                    strcpy(pcTmp,num[0]);
                    strcat(pcTmp,".");
                    strcat(pcTmp,num[1]);
                    ret =0;
                }
                else
                {
                    ret = -1;
                }
                
            }
            else
            {
                printf("\nERR-GET_PCT : line(2) of modem response does not have pcTmp info\n");
                ret = -1;
            }            
        }
        else
        {
            printf("\n-----error in mdm_get_pcTmp()-----\n");
            ret = -1;
        }
    }
    else
    {
        ret = -1;
    }
    return ret;

}

int mdm_get_imsi(int fd, char *imsi)
{
    char rd_str[800]={0};
    int tm = 0;
    fd_ctrl(fd,2,NULL);
    fd_ctrl(fd,2,NULL);
    int rt_rdwr = write(fd,RTR_SIM_IMSI,strlen(RTR_SIM_IMSI));
    int sts = 0;
    int ret = 0;

    if (rt_rdwr > 0)
    {
        memset(rd_str,0,strlen(rd_str));
        do
        {
            msleep(100);
            rt_rdwr = read(fd,rd_str+(strlen(rd_str)),70);
            if(strstr(rd_str,"AT+CIMI"))
            {
                if( strstr(rd_str,"OK\r\n") )
                {
                    sts = 1;  
                    // printf("strlen(rd_str) = %d\n",strlen(rd_str));
                    // printf("rd_str : %s\n",rd_str);
                    break;
                }
            }
            
        } while ((tm++) < 10);
        if(sts == 1)
        {
            char *lns[5] = {0};
            sts = split_line(rd_str,lns,5);
            int i=0,ii=0;
            char *numstr[2] = {0};
            for(i=0;i<sts;i++)
            {
                ii = str2numstr(lns[i],numstr,1,0);
                if(ii == 1)
                {
                    // sts = 1;
                    // i+=1;
                    // puts(lns[i]);
                    if(strlen(numstr[0])>5)
                    break;
                }
            }
            if(ii == 1)
            {
                memset(imsi,0,strlen(imsi));
                // for(int i=0;i<sts;i++)
                // {
                //     printf("l%d : %s\n",i,lns[i]);
                // }
                // char *numstr[2] = {0};
                // sts = str2numstr(lns[i],numstr,1,0);
                strcpy(imsi,numstr[0]);
                rmv_nlcr(imsi);
                ret = 0;
            }
            else
            {
                dbg_print(Bold_Red,
                    "\nERR-GET_IMSI : modem response does not have imsi info\n");
                ret = -1;
            }            
        }
        else
        {
            printf("\nERR-GET_IMSI : Error from modem.\n");
            ret = -1;
        }
    }
    else
    {
        ret = -1;
    }
    return ret;
}


int mdm_get_spn(int fd, char *spn)
{
    char rd_str[700]={0};
    int tm = 0;
    fd_ctrl(fd,2,NULL);
    fd_ctrl(fd,2,NULL);
    int rt_rdwr = write(fd,RTR_SIM_SPN,strlen(RTR_SIM_SPN));
    int sts = 0;
    int ret = 0;

    if (rt_rdwr > 0)
    {
        do
        {            
            rt_rdwr = read(fd,rd_str+(strlen(rd_str)),70);
            if( strstr(rd_str,"OK\r\n") )
            {
                if(strstr(rd_str,"+CSPN:"));
                {
                    sts = 1;
                    // printf("strlen(rd_str) = %d\n",strlen(rd_str));
                    // printf("rd_str : %s\n",rd_str);
                    break;
                }
            }
            msleep(100);            
        } while ((tm++) < 10);
        if(sts == 1)
        {
            char *lns[5] = {0};
            sts = split_line(rd_str,lns,5);
            if(sts > 2)
            {
                memset(spn,0,strlen(spn));
                // for(int i=0;i<sts;i++)
                // {
                //     printf("l%d : %s\n",i,lns[i]);
                // }
                rmv_nlcr(lns[1]);
                char *tmp = strstr(lns[1],": ");
                if(tmp != NULL)
                {
                    strcpy(spn,tmp+2);
                    ret = 0;
                }
                else
                {
                    ret = -1;
                }
            }
            else
            {
                printf("\nERR-GET_SPN : line(2) of modem response does not have SPN info\n");
                ret = -1;
            }            
        }
        else
        {
            printf("\n-----error in mdm_get_spn()-----\n");
            ret = -1;
        }
    }
    else
    {
        ret = -1;
    }
    return ret;
}

/*****************************************
 * get the response of the modem for a valid AT command
 * ARGUMENTS : 
 *      fd : file descriptor of ttyAT
 *      cmd : string, contains the command
 *      rsp : string, will filled with the response <min size 150>
 *      sz_rsp : size of the string(rsp) <Minimum 100>
 *      tm_out : Time out in milli-Sec
 * RETURN : 
 *      Success : 0 {OK from the modem}
 *      Error   : -1{ ERROR from the modem}
 * **************************************/
int mdm_rsp(int fd, char *cmd, char *rsp, \
            uint32_t sz_rsp, uint64_t tm_out)
{
    int ret=0, sts=0, sz=0;
    int cnt = 30;
    char *p1=NULL, *p2=NULL;
    fd_ctrl(fd,2,NULL);
    fd_ctrl(fd,2,NULL);
    if(sz_rsp < 100)
    {
        dbg_print(Bold_Red,"ERR : sz_rsp < 100");
        dbg_print(Bold_Green,"Exiting... .. .\n");
        return -1;
    }
    memset(rsp,0,sz_rsp);
    ret = write(fd,cmd,strlen(cmd));
    tm_out /= 100;
    if(ret > 0)
    {
        do
        {
            ret = read(fd,rsp+sz,cnt);
            sz = strlen(rsp);
            if( strstr(rsp,"OK\r\n") )
            {
                sts = 1;
                // break;
            }
            else if(strstr(rsp,"ERROR\r\n"))
            {
                sts = -1;
            }
            msleep(100);
        } while ( (tm_out-- > 0)  && (sz < sz_rsp) && (sz+cnt <= sz_rsp)\
                   && (sts==0) );
        //  
    }
    if(sts == 1)
    {
        ret = 0;
    }
    else
    {
        ret = -1;
    }
    return ret;
}

int mdm_get_ccid(int fd, char *ccid)
{
    char rd_str[800]={0};
    int tm = 0;
    int sts = 0;
    int ret = 0;
    fd_ctrl(fd,2,NULL);
    fd_ctrl(fd,2,NULL);
    int rt_rdwr = write(fd,RTR_SIM_CCID,strlen(RTR_SIM_CCID));

    if (rt_rdwr > 0)
    {
        do
        {            
            rt_rdwr = read(fd,rd_str+(strlen(rd_str)),70);
            if( strstr(rd_str,"OK\r\n") )
            {
                sts = 1;  
                // printf("strlen(rd_str) = %d\n",strlen(rd_str));
                // printf("rd_str : %s\n",rd_str);
                break;
            }
            msleep(100);
        } while ((tm++) <= 10);
        if(sts == 1)
        {
            char *lns[5] = {0};
            char *tmp = NULL;
            sts = split_line(rd_str,lns,5);
            if(sts > 2)
            {
                memset(ccid,0,strlen(ccid));
                // for(int i=0;i<sts;i++)
                // {
                //     printf("l%d : %s\n",i,lns[i]);
                // }
                for(int i=0;i<sts;i++)
                {
                    rmv_nlcr(lns[i]);
                    tmp = strstr(lns[i],": ");
                    if(tmp)
                        break;
                }
                
                if(tmp != NULL)
                {
                    strcpy(ccid,tmp+2);
                    ret = 0;
                }
                else
                {
                    ret = -1;
                }                
            }
            else
            {
                dbg_print(Bold_Red,\
                    "\nERR-GET_CCID : modem response does not have ccid info\n");
                ret = -1;
            }            
        }
        else
        {
            dbg_print(Bold_Red,\
                "\n-----mdm_get_ccid() :: modem response : error-----\n");
            ret = -1;
        }
    }
    else
    {
        ret = -1;
    }
    return ret;
}

/***********************************************
 * Get the response of a terminal session with
 * splitted line.
 * Return : 
 *  Success : +ve (Line count)
 *  Error   : -1
 * ********************************************/
int rsp_popen(const char *cmd, char *rsp, \
            char *rsp_ln[], uint32_t ln_max, uint32_t sz_rsp)
{
    FILE *fp = NULL;
    fp = popen(cmd,"r");
    size_t ret;
    int ln_cnt = -1;
    if(fp == NULL)
    {
        dbg_print(Bold_Red,"\n\rpopen() failed due to : %s\n",strerror(errno));
        dbg_print(Bold_Red,"\nExiting ... .. .\n\r");
        pclose(fp);
        return -1;
    }
    else
    {
        ret = fread_unlocked((void*)rsp,1,sz_rsp,fp);
        // printf("size rsp = %d\n",strlen(rsp));
        // printf("fread_unlocked() = %d\n",ret);
        ln_cnt = split_line(rsp,rsp_ln,ln_max);
        // printf("split_line() = %d\n",ln_cnt);
        //printf("--------\n%s\n%s\n------\n",rsp_ln[0],rsp_ln[1]);
    }
    pclose(fp);
    return ln_cnt;    
}


int mdm_get_netSts(void)
{
    char rsp[500] = {0};
    char *rsp_ln[15] = {0};
    int ret = rsp_popen("cm data",rsp,rsp_ln,15,500);
    int sts = -1;
    if(ret > 0)
    {
        for(int i=0;i<ret;i++)
        {
            if(strstr(rsp_ln[i],"Connected:"))
            {
                if(strstr(rsp_ln[i],"yes"))
                {
                    sts = 1;
                }
                else
                {
                    sts = 0;
                }
            }
        }
    }
    return sts;
}


int mdm_get_netSts_withIP(void)
{
    FILE *fp = NULL;
    char str[4096] = {0};
    char *lns[5] = {0};
    char *numstr[2] = {0};
    char *num1[4] = {0};
    char *num2[4] = {0};
    int n1[4] = {0};
    int n2[4] = {0};
    char *p1 = NULL, *p2 = NULL;
    int ret = 0,sts = 0,ret1 = 0;

    fp = popen("ifconfig", "r");
    if(fp == NULL )
    {
        printf("popen(fp) failed due to : %s\n",strerror(errno));
        printf("exiting....\n");
        ret = -1;
    }
    else
    {
        ret1 = fread((void*)str,1,4096,fp);
        if(ret1 > 0)
        {
            p1 = strstr(str,"rmnet_data0");
            if(p1 != NULL)
            {
                p2 = strstr(p1,"inet addr:");
                if(p2 == NULL)
                {
                    pclose(fp);
                    return 0;
                }
                sts = split_line(p2,lns,5);
                if(sts > 0)
                {  
                    str2numstr(lns[0],numstr,2,1);
                    // printf("ip[0] = %s\n",numstr[0]); 
                    // printf("ip[1] = %s\n",numstr[1]); 
                    str2numstr(numstr[0],num1,4,0);
                    str2numstr(numstr[1],num2,4,0);
                    for(int i = 0; i<4 ;i++)
                    {
                        n1[i] = atoi(num1[i]);
                        n2[i] = atoi(num2[i]);
                    }
                    if((n1[0] <= n2[0]) && ( n1[1] <= n2[1]) && (n1[2] <= n2[2]))
                    {
                        // printf("success\n");
                        ret = 1;
                    }
                    else
                    {
                        // printf("error\n");
                        ret = 0;
                    }
                }
                else
                {
                    ret = 0;
                }
            }
            else
            {
                ret = 0;
            }
        }
        else
        {
            ret = 0;
        }
    }
    pclose(fp);
    return ret;
}

/*********************************
 * Get the current radio access technology of the modem
 * ******************************/
int mdm_get_rat(int fd, char *rat, uint32_t sz_rat)
{
    char str[150] = {0};
    int ret = 0;
    char *ptr = NULL;
    char *lns[5] = {0};
    memset(rat,0,sz_rat);
    ret = mdm_rsp(fd,RTR_GET_RAT, str,100,2000);
    if(ret == 0)
    {
        ret = split_line(str,lns,5);
        if(ret > 0)
        {
            for(int i=0;i<ret;i++)
            {
                ptr = strstr(lns[i],"RAT: ");
                if(ptr)
                {
                    ptr += strlen("RAT: ");
                    sprintf(rat,"%s",ptr);
                    ret = 0;
                    break;
                }
            }
            if(ptr==NULL)
            {
                ret = -1;
            }
        }
        else
        {
            ret = -1;
        }
    }
    else
    {
        ret = -1;
    }
    return ret;
}

int get_fmw_ver(int fd, FMW_VER_ *ver)
{
    int fp,ret;
    char str[420]={0};
    char tmp[20] ={0};
    char cKlog[150] = {0};
    char *p1 = NULL;
    char *p2 = NULL;
    fp = open("/etc/legato/version", O_RDONLY);
    if (fp < 0) 
    { 
        printf("open() failed due to : %s\n",strerror(errno));
        printf("Exiting ... .. .\n");
        close(fp);
        return -1;
    } 
    ret = read(fp,(void *)str, 410);
    if (ret < 0) 
    { 
        printf("read() failed due to : %s\n",strerror(errno));
        printf("Exiting ... .. .\n");
        close(fp);
        return -1;
    } 
        
    strcpy(tmp,"version: ");
    p1 = strstr(str,tmp);
    ret = strlen(tmp);
    p1 += ret ;
    p2 = strchr(p1,'\n');
    *p2 = 0;
    p2 += 1;
    strcpy(ver->v_fs,p1);
    memset(tmp,0,ret);
    strcpy(tmp,"linux-quic-quic: ");
    ret = strlen(tmp);
    p1 = strstr(p2,tmp);
    p1 += ret;
    p2 = strchr(p1,'\n');
    *p2 = 0;
    strcpy(ver->v_krn,p1);

    close(fp);

    FILE *fs;
    memset(str,0,400);
    memset(ver->v_app_inv,0,sizeof(ver->v_app_inv));
    strcpy(ver->v_app_inv,APP_VER);

    ret = mdm_get_fmwv(fd,str);
    if(ret == 0)
    {
        strcpy(ver->v_mdm_fmw,str);
    }
    memset(str,0,400);

    fs = popen("/legato/systems/current/bin/app version RelCellularManagerApp", "r");
    if(fs == NULL)
    {
        dbg_print(Bold_Red,"Error : popen() failed in get_fmw_ver() due to : %s\n",\
                strerror(errno));
        sprintf(cKlog,"ERR-RTR-BRZ : popen() failed in get_fmw_ver() due to : %s\n",strerror(errno));
        wr_klog(cKlog);
        dbg_print(Bold_Red,"Exiting(get_fmw_ver())... .. .\n");
        pclose(fs);
        return -1;
    }
    
    
    ret = fread(str,1,100,fs);
    memset(cKlog,0,150);
    sprintf(cKlog,"RTR-BRZ : FMWV : popen() = %d\n",ret);
    wr_klog(cKlog);
    if(ret > 0)
    {
        char *numstr[2];
        ret = str2numstr(str,numstr,1,1);
        memset(cKlog,0,strlen(cKlog));
        sprintf(cKlog,"RTR-BRZ : fmwv : str2numstr() = %d\n",ret);
        wr_klog(cKlog);
        if(ret == 1)
        {
            strcpy(ver->v_app_rls,numstr[0]);
            memset(str,0,200);
            ret = 0;
            // ret = mdm_get_fmwv(fd,str);
            // if(ret == 0)
            // {
            //     strcpy(ver->v_mdm_fmw,str);
            // }
        }
        else
        {            
            printf("ERR : Could not get 'Relysis FMW_VER\n");
            printf("Exiting... .. .\n");
            pclose(fs);
            return -1;
        }        
    }
    pclose(fs);
    close(fp);
    return ret;
}

/*****************************
 * Get the sim availability in the active slot
 * Return : 
 *      Success : 1
 *      Error   : 0
 * **************************/
int mdm_get_sltSts(void)
{
    int ret, sz_str=100;
    char str[100];
    char *lns[3] = {0};
    ret = rsp_popen("cm sim",str,lns,3,sz_str);
    if(ret > 0)
    {
        sz_str = 0;
        for(int i=0;i<ret;i++)
        {
            printf("\nIn mdm_get_sltSts(%d) : %s\n",read_active_sim(),lns[i]);
            if( strstr(lns[i],"LE_SIM_READY") \
                || strstr(lns[i],"LE_SIM_INSERTED") )
            {
                sz_str = 1;
                break;
            }
        }
    }
    else
    {
        sz_str = 0;
    }
    return sz_str;    
}


int mdm_get_rstIntrvl(char *rst_intrvl,uint32_t sz_rst_intrvl)
{
    int fp = 0,ret = 0,line = 0,i = 0,len = 0,j = 0;
    char *numstr[10]={0};
    char str[650]={0};
    char *dest[22]={0};
    int num[6] = {0};
    char *p1 = NULL;

    if(sz_rst_intrvl < 25)
    {
        dbg_print(Bold_Red,"Err in mdm_get_rstIntrvl(). minimum size = 25\n");
        dbg_print(Bold_Red,"Returning... .. .\n");
        return -1;
    }

    memset(rst_intrvl,0,sz_rst_intrvl);

    fp = open("/etc/crontab", O_RDONLY);
    if (fp < 0) 
    { 
        dbg_print(Bold_Red,"open() failed due to : %s\n",strerror(errno));
        dbg_print(Bold_Red,"Exiting\n");
        close(fp);
        return -1; 
    } 
    ret = read(fp,(void *)str, 600);
    if (ret < 0) 
    { 
        printf("read() failed due to : %s\n",strerror(errno));
        close(fp);
        return -1; 
    } 
    //printf("%s\n",str);
    line = split_line(str,dest,20);
    if (line > 0)
    {
        for(i=0;i<line;i++)
        {
            p1 = strstr(dest[i],"MaintenanceReboot");
            if(p1 != NULL)
            {
                len = str2numstr(dest[i],numstr,5,1);
                for(j=0; j<len; j++)
                {
                    num[j] = atoi(numstr[j]);
                }
                sprintf(rst_intrvl,"%02d-%02d %02d:%02d",num[2],num[3],num[1],num[0]);
                // printf("RST_TIME = %s\n",rst_intrvl);
                ret = 0;
                break;
            }
            else if(p1 == NULL)
            {
                ret = -1;
            }
        }
    }
    else
    {
        ret = -1;
    }
    // printf("----Returning---from mdm_get_rstIntrvl()---\n");
    close(fp);
    return ret;
}

/*****************************
 * Get the sim availability in the active slot
 * Return : 
 *      Success : 1
 *      Error   : 0
 * **************************/
int mdm_get_Slt_Sts(int fd)
{
    char str[130] = {0};
    char *numstr[2] = {0};
    int ret = 0, num = 0, cnt = 0;
    char *ptr = NULL, *p1 = NULL;

    do
    {
        memset(str,0,strlen(str));
        ret = mdm_rsp(fd,RTR_QRY_KSREP,str,130,2000);
        cnt++;
        p1 = strstr(str,"+KSREP:");
        // printf("p1 = %s\n",p1);
        msleep(100);
    } while ((p1 == NULL) && (cnt < 3));

    if(ret == 0)
    {
        ptr = strstr(str,"+KSREP:");
        if(ptr)
        {
            str2numstr(ptr,numstr,2,0);
            num = atoi(numstr[1]);
            switch (num)
            {
            case RTR_SLT_STS_RDY:
                ret = 1;
                dbg_print(Bold_Yellow,"SLT_STS: SLT_STS_RDY\n");
                break;
            case RTR_SLT_STS_WAIT:
                ret = 0;
                dbg_print(Bold_Yellow,"SLT_STS: SLT_STS_WAIT\n");
                break;
            case RTR_SLT_STS_ABSNT:
                ret = 0;
                dbg_print(Bold_Yellow,"SLT_STS: SLT_STS_ABSNT\n");
                break;
            case RTR_SLT_STS_LKD:
                ret = 0;
                dbg_print(Bold_Yellow,"SLT_STS: SLT_STS_LKD\n");
                break;
            case RTR_SLT_STS_ERR:
                ret = 0;
                dbg_print(Bold_Yellow,"SLT_STS: SLT_STS_ERR\n");
                break;
            case RTR_SLT_STS_UNWN:
                ret = 0;
                dbg_print(Bold_Yellow,"SLT_STS: SLT_STS_UNWN\n");
                break;       
            default:
                dbg_print(Red,"case didn't match\n");
                ret = 0;
                break;
            }
        }
        else
        {
            dbg_print(Bold_Red,"ERR-SLTsTS : mdm_get_Slt_Sts() failed.\n");
            ret = 0;
        }
        
    }
    else
    {
        dbg_print(Bold_Red,"ERR-SLTsTS : mdm_get_Slt_Sts() failed <no rsp from mdm>.\n");
       ret = 0; 
    }
    return ret;
}