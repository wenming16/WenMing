/*=======================================================================
 *Subsystem:   裸机
 *File:        Task_BalanceControl.C
 *Author:      WenM
 *Description: 通信：
               接口：
               波特率：
 ========================================================================
 * History:    修改历史记录列表
 * 1. Date:   
      Author:  
      Modification:  
      
========================================================================*/

#include  "Task_BalanceControl.h"
#include  "WorkModeJudge.h" 
#include  "LTC6811_ConnectType.h"
#include  "Task_DataProcess.h"
#include  "Task_FltLevJudg.h"
#include  "LTC6811_VoltCollect.h"
#include  "Task_Init.h"

#include  "LTC6811_PassBalance.h"

/*=======================================================================
 *函数名:      BalanceControl_Strategy(flaot, uint8, uint16, uint32, uint8, float)
 *功能:        对电池组进行被动均衡
               均衡控制策略:当最大单体大于平均电压balancevolt时进行均衡
 *参数:        curr：判断电流的大小,只当电流小于3A(静态)进行均衡
               faultflg:  在不出现故障的情况下才进行均衡
               voltmax：  最大电压
               totalvolt：25串电池的总压,用于求平均电压
               balacenod: 均衡的节点                 
 
 *返回：       uint8:     0表示正常均衡
                          1表示均衡出现错误参数并停止均衡
                          2表示未达到均衡条件不进行均衡
 *说明：       被动均衡函数
========================================================================*/
static
uint8 BalanceControl_Strategy(float curr, uint8 faultflg, uint16 voltmax, uint32 totalvolt, uint8 balacenod, uint16 balancevolt)
{
  uint8 tskstate=2;                   //返回2表示未进行均衡
  static uint16 cnt;
  uint8 balanceNum;
  //static uint8 balanceflag; //均衡开启指令确保只发一次
  
  if(abs(curr)>=10 && (faultflg == 0) && (g_WorkStateJudge.WorkState == MODE_CHARGE)) //只有在充电的过程中电流大于5A才开启
  {
    if((voltmax - (totalvolt/25.0)) > balancevolt)
    {
       if(++cnt*BALANCEPERIO/1000>2)//持续2s,连续发命令是否会出错?
       {
         cnt=2000/BALANCEPERIO;
         if(balacenod <= NUM1_Batper_true)
         {
            tskstate   = LTC6811_BalanceControl(balacenod, 0x00, 0x00, 1); 
         }
         else if(balacenod <= (NUM1_Batper_true+NUM2_Batper_true))
         {
            balanceNum = balacenod-NUM1_Batper_true;
            tskstate   = LTC6811_BalanceControl(0x00, balanceNum, 0x00, 1);
         }
         else if(balacenod <= (NUM1_Batper_true+NUM2_Batper_true+NUM3_Batper_true))
         {
            balanceNum = balacenod-NUM1_Batper_true-NUM2_Batper_true;
            tskstate   = LTC6811_BalanceControl(0x00, 0x00, balanceNum, 1);
         }
         else
         {
            return 1;
         }
         //balanceflag = 1;
       }
    }
    else
    {
      cnt = 0;
    }
  }
  else
  {
     //balanceflag = 0;
     tskstate = LTC6811_BalanceControl(0x00, 0x00, 0x00, 0); //未满足条件不进行均衡 
     return 2;
  }
  return tskstate;
}

/*=======================================================================
 *函数名:      Task_BalanceControl_ON(void)
 *功能:        均衡开启控制任务
 *参数:        无       
 *返回：       无
 *说明：       根据均衡控制策略开启均衡控制,只在电压采集完毕之后开启均衡
========================================================================*/
void Task_BalanceControl_ON(void)
{
   uint8 balancestate;
   balancestate = BalanceControl_Strategy(g_DataColletInfo.DataCollet_Current_Filter, g_Flt_Charge.Level_Charge_BalanceOff_Flag,\
                                          g_LTC6811_VoltInfo.CellVolt_Max, g_LTC6811_VoltInfo.CellVolt_Total, g_LTC6811_VoltInfo.CellVolt_MaxNode, 500);

  g_Roll_Tick.Roll_BalanOn++;
}
/*=======================================================================
 *函数名:      Task_BalanceControl_OFF(void)
 *功能:        均衡关闭控制任务
 *参数:        无       
 *返回：       无
 *说明：       在电压采集之前关闭均衡开关
========================================================================*/
void Task_BalanceControl_OFF(void)
{
   LTC6811_BalanceControl(0x00, 0x00, 0x00, 0);//关闭均衡功能
   
   g_Roll_Tick.Roll_BalanOff++;
}