/*=======================================================================
 *Subsystem:   裸机
 *File:        Task_SOC.c
 *Author:      WenYuhao
 *Description: 
 ========================================================================
 * History:    
 * 1. Date:
      Author:
      Modification:  
========================================================================*/

#include  "TypeDefinition.h"
#include  "Task_SOC.h"
#include  "BattInfoConfig.h"
#include  "BattInfoParaStructure.h"
#include  "Task_DataProcess.h"
#include  "stdtypes.h"
#include  "ADC.h"
#include  "ADC_Config.h"
#include  "Filter_Function.h"
#include  "Roll_Task.h"
#include  "Task_BmsAndUpMonitor.h"
#include  "WorkModeJudge.h"

SOCInfo_T     SOCInfo;
EnergyInfo_T  EnergyInfo;


static float  inition_soc(float v);
static uint8  SOC_StaticTime(uint8 readhour, uint8 readminite, float current);
static void   SOC_AhIntegral(float current, uint16 Voltagemin, uint16 Voltagemax, uint16 SampleTime);
static float  Energy_TotalCal(uint8 mode,float ah1,uint32 Total_V,float Total_E,float current);
/*=======================================================================
 *函数名:      Task_SOC(void)
 *功能:        SOC的计算函数
 *参数:        无       
 *返回：       无
 *说明：       计算所需要的SOC值
========================================================================*/
void Task_SOC(void)                                
{       
  uint8 Value=0;    
  
  //采集电流
  DataColletInfo.DataCollet_Current_Filter = FilterFunction_Median(ADC_Current, DataColletInfo.DataCollet_Current_Filter);// 取出真实电流值，并做滤波
  
  //SOC的估算 
  if(VoltInfo.CellVolt_Min != 0 || VoltInfo.CellVolt_Max != 0xFFFF)     // 数据有效性判断
  {                                    
    //SOCInfo.SOC_CheckTable_Flag = SOC_StaticTime(24, 60, DataColletInfo.DataCollet_Current_Filter);//常电下电流小于0.5A时查表
    SOC_AhIntegral(DataColletInfo.DataCollet_Current_Filter, VoltInfo.CellVolt_Min, VoltInfo.CellVolt_Max, SOC_Period); //计算SOC值 
  }
  else
  {
    SOCInfo.SOC_Init = 0;
  }
}

/*=======================================================================
 *函数名:      SOC_AhIntegral(float current, uint16 Voltagemin, uint16 Voltagemax, uint16 SampleTime)
 *功能:        SOC安时积分法计算
 *参数:        Volatagemin：单体最小电压
               Voltagemax：单体最大电压
               SampleTime，函数执行时间
 *返回：       无
 *说明：       SOCInfo.SOC_ValueRead：SOC上传至显示屏上的读取值和SOC上次存储值
               soc_Volt：按最大最小电压取权值得到的SOC值
               思路：为了避免上次存储值和查表得到的校正值产生大的偏差，
                     采用加权重的办法将存储值和查表值进行偏移，开始时偏
                     向存储值，到后期偏向查表值。存储在充电时期且SOC>90%
                     或SOC<20%是进行校正。
========================================================================*/  
static
uint8 QQ[5];
static
void SOC_AhIntegral(float current, uint16 Voltagemin, uint16 Voltagemax, uint16 SampleTime)  //100ms
{        
  float T=0;
  float AH=0,ah1=0;
  float Vmin = 0, Vmax = 0;
  static float Cellcap;
  Vmin = Voltagemin/10000.0;
  Vmax = Voltagemax/10000.0;
  T = SampleTime/1000.0;// 100ms时间转变为秒
       
  if((SOCInfo.SOC_Init <= 0)||(SOCInfo.SOC_Init >= 100)) //EFPROM里的值为0时，查初值(首次下载用到)
  { 
      //SOC_LEP_DATA.SOC_t = 1;                         //CAN外发标记
      SOCInfo.SOC_LowestVoltGet = inition_soc(Vmin);        //按最小电压查表得到的SOC 
      SOCInfo.SOC_HighestVoltGet = inition_soc(Vmax);       //按最大电压查表得到的SOC 
      SOCInfo.SOC_ValueVoltGet = SOCInfo.SOC_LowestVoltGet/(1+SOCInfo.SOC_LowestVoltGet-SOCInfo.SOC_HighestVoltGet);
      SOCInfo.SOC_ValueRead = SOCInfo.SOC_ValueVoltGet;  //首次初始化SOC值，首次的一致性较好  
      SOCInfo.SOC_ValueInitDiff = 0; 
      SOCInfo.SOC_ValueRealtimeDiff = SOCInfo.SOC_ValueInitDiff ;          
      SOCInfo.SOC_Init =  (uint16)(SOCInfo.SOC_ValueRead*100);
      *(uint16*)(0x0D10) =  (uint16)(SOCInfo.SOC_Init); //SOCInfo.SOC_Init

  }
  else if(((SOCInfo.SOC_ValueRead < 0.2)||(SOCInfo.SOC_ValueRead > 0.9)) && ((SOCInfo.SOC_StandingTime >= 4)||(SOCInfo.SOC_CheckTable_Flag == 1)))
  {   //当SOC小于20%或大于90%,且电池静置时间大于3小时,或在常电状态下电流小于0.5A持续时间大于3小时进行查表     
      //SOC_LEP_DATA.SOC_t = 2;
      SOCInfo.SOC_StandingTime = 0; 
      SOCInfo.SOC_CheckTable_StartTime = ((uint16)24<<8) | (60);      
      SOCInfo.SOC_LowestVoltGet = inition_soc(Vmin);        //按最小电压查表得到的SOC 
      SOCInfo.SOC_HighestVoltGet = inition_soc(Vmax);       //按最大电压查表得到的SOC 
      SOCInfo.SOC_ValueVoltGet = SOCInfo.SOC_LowestVoltGet/(1+SOCInfo.SOC_LowestVoltGet - SOCInfo.SOC_HighestVoltGet);
      SOCInfo.SOC_ValueInitDiff  = SOCInfo.SOC_ValueRead - SOCInfo.SOC_ValueVoltGet;
      SOCInfo.SOC_ValueRealtimeDiff = SOCInfo.SOC_ValueInitDiff ;
  } 
   
  //安时积分，需要避免大数吃小数
  if(current>=0.5 || current<=-0.5)
  {  
    //if(SOCInfo.SOC_CalTime % 300 == 0)//5min根据温度查一次容量
    //{
    //   SOCInfo.SOC_CalTime = 0;
    //   Cellcap = CellCapacity_Cal(g_Batt_TempMesg.aveTemp);  
    //}
    Cellcap = CELL_CAPACITY;
    
    AH = current*T/3600.0/Cellcap;  //SOC值  
    ah1 = current*T/3600.0;
  }
  //充电状态     
  if(g_WorkStateJudge.WorkState == MODE_CHARGE)    //充电 
  {      
    if(VoltInfo.CellVolt_Max >= (CELL_VOLT_MAX-0.01)*10000)                         
    //if(VoltInfo.CellVolt_Max >= (MaxVolt_Cal(g_Batt_TempMesg.aveTemp)-0.02)*10000)          //当电压达到(最大值-0.02)时，进行充电末端的校正    
    {     
      SOCInfo.SOC_HighestVoltGet = 1.0;                             // 高电压单体SOC先为1；
      if(SOCInfo.SOC_ValueRead > SOCInfo.SOC_ValueVoltGet)          //充电状态max_V达到(最大值-0.01)V时,R>V,重置SOC_deta和SOC_K的值
      {                                                             //soc_v未置1前soc_r>soc_v,当第二次进入此函数时soc_v=1,此时soc_v>soc_r
        SOCInfo.SOC_ValueRealtimeDiff = SOCInfo.SOC_ValueRead - 1; 
        SOCInfo.SOC_ValueInitDiff  =  SOCInfo.SOC_ValueRealtimeDiff;
      }
      else if((1.0-SOCInfo.SOC_ValueVoltGet > 0.01) && (SOCInfo.SOC_HighestVoltGet>=1.0)) //充电状态max_V达到(最大值-0.02)V时,R<V,重置SOC_deta和SOC_K的值 
      {
        SOCInfo.SOC_ValueRealtimeDiff = SOCInfo.SOC_ValueRead - 1; 
        SOCInfo.SOC_ValueInitDiff  =  SOCInfo.SOC_ValueRealtimeDiff;
      }      
    }             
   
    EnergyInfo.Energy_Total_Charge = Energy_TotalCal(g_WorkStateJudge.WorkState,ah1,DataColletInfo.SysVolt_Total,EnergyInfo.Energy_Total_Charge,current);
    //EnergyInfo.Energy_Once_DisCharge = 0;
  }            
  //放电状态
  else     
  {   
    if(VoltInfo.CellVolt_Min <= CELL_VOLT_MIN*10000.0)//3.0V     
    {     
      SOCInfo.SOC_LowestVoltGet = 0;                                                    //低电压的SOC先为0
      if(SOCInfo.SOC_ValueRead < SOCInfo.SOC_ValueVoltGet)                              //防止S_V变为0时，S_R跳变为0
      {
        SOCInfo.SOC_ValueRealtimeDiff = SOCInfo.SOC_ValueRead;
        SOCInfo.SOC_ValueInitDiff  = SOCInfo.SOC_ValueRealtimeDiff;
      }
      else if((SOCInfo.SOC_ValueVoltGet > 0.01) && (SOCInfo.SOC_LowestVoltGet <= 0))      //防止S_V变为0时，S_R向下跳变
      {
        SOCInfo.SOC_ValueRealtimeDiff = SOCInfo.SOC_ValueRead;
        SOCInfo.SOC_ValueInitDiff  = SOCInfo.SOC_ValueRealtimeDiff;
      }
    }
    EnergyInfo.Energy_Total_DisCharge = Energy_TotalCal(g_WorkStateJudge.WorkState,ah1,DataColletInfo.SysVolt_Total,EnergyInfo.Energy_Total_DisCharge,current);          
                
    //SOC_LEP_DATA.ThisTime_Energy = SOC_LEP_DATA.ThisTime_Energy + Out_Energy;    
    
  } 
  
  SOCInfo.SOC_LowestVoltGet  = SOCInfo.SOC_LowestVoltGet-AH;        //按查表的最小电压累加SOC
  SOCInfo.SOC_HighestVoltGet = SOCInfo.SOC_HighestVoltGet-AH;       //按查表的最大电压累加SOC
  
  
  if(SOCInfo.SOC_HighestVoltGet >= 1)
     SOCInfo.SOC_HighestVoltGet = 1;
  else if (SOCInfo.SOC_LowestVoltGet <= 0)
     SOCInfo.SOC_LowestVoltGet = 0;  
  
  //充电偏向于电压高的电池的SOC，放电偏向于低电压电池的SOC
  SOCInfo.SOC_ValueVoltGet = SOCInfo.SOC_LowestVoltGet / (1 + SOCInfo.SOC_LowestVoltGet - SOCInfo.SOC_HighestVoltGet); 
  SOCInfo.SOC_ValueRead = SOCInfo.SOC_ValueVoltGet + SOCInfo.SOC_ValueRealtimeDiff;         //按上次读取的SOC累加SOC   

  if(SOCInfo.SOC_CalTime % 10==0) //SOC_Read校正
  {
    SOCInfo.SOC_CalTime = 0;
    if((abs(current)>=0.5) && ((abs(SOCInfo.SOC_ValueRealtimeDiff))>=0.001))
    {
       if( (g_WorkStateJudge.WorkState == MODE_CHARGE) && ((SOCInfo.SOC_ValueVoltGet >= 0.9)))     //充电，S_V>0.9
       {
         if(SOCInfo.SOC_ValueVoltGet < 1)
         {QQ[0]++;
            SOCInfo.SOC_ValueRealtimeDiff = SOCInfo.SOC_ValueRealtimeDiff - (abs(current)/(400.0*SYS_CAPACITY))*SOCInfo.SOC_ValueInitDiff ;    //S_V>0.9 && S_V<1     
         } 
         else  
         { QQ[1]++;
            SOCInfo.SOC_ValueRealtimeDiff = SOCInfo.SOC_ValueRealtimeDiff - (1.0/180.0)*SOCInfo.SOC_ValueInitDiff ;     //S_V>=1
         }
       }
       else if((SOCInfo.SOC_ValueVoltGet<0.2) && (g_WorkStateJudge.WorkState == MODE_DISCHARGE))        //放电，S_V<0.2
       {
         if(SOCInfo.SOC_ValueVoltGet > 0)
         {  QQ[2]++;
            SOCInfo.SOC_ValueRealtimeDiff = SOCInfo.SOC_ValueRealtimeDiff - (abs(current)/(400.0*SYS_CAPACITY))*SOCInfo.SOC_ValueInitDiff ;    //S_V>0 && S_V<0.2 
         } 
         else  
         {  QQ[3]++;
            SOCInfo.SOC_ValueRealtimeDiff = SOCInfo.SOC_ValueRealtimeDiff - (abs(current)/(120.0*SYS_CAPACITY))*SOCInfo.SOC_ValueInitDiff ;    //S_V<=0
         }      
       }
       else
       {   QQ[4]++;
          SOCInfo.SOC_ValueRealtimeDiff = SOCInfo.SOC_ValueRealtimeDiff - (abs(current)/(1200.0*SYS_CAPACITY))*SOCInfo.SOC_ValueInitDiff ;     //充电S_V 0~0.9 || 放电S_V 0.2~1
       }
    }
  }
  if(SOCInfo.SOC_ValueRead >= 1) 
  {
     SOCInfo.SOC_ValueRead = 1;
  }
  else if(SOCInfo.SOC_ValueRead <= 0) 
  {
     SOCInfo.SOC_ValueRead = 0;
  }     
  //EEprom_Data.Charge_Times = EEprom_Data.Charge_Times + (uint16)(EnergyInfo.Energy_Total_Charge/SysEnergy); //充电次数 
  SOCInfo.SOC_CalTime++; 
} 

/*=======================================================================
 *函数名:      CellCapacity_Cal
 *功能:        不同温度下电池的容量不一致
 *参数:        temp：按电池箱体的平均温度                
 *返回：       float：在此温度下实验得到的电池容量
 *说明：       在不同的温度下电池的容量不一致
========================================================================*/ 
float CellCapacity_Cal(uint8 tempave)
{
  uint8 i;
  float cap[6] = {26.1196, 32.4033, 36.3806, 41.9106, 44.5642, 47.2677}; //电池容量
  float temp[6] = {   -20,     -10,       0,      10,      25,      45}; //电池温度
  
  if(tempave<=temp[0])
  {
    return cap[0];
  }
  
  else if(tempave>=temp[5])
  {
    return cap[5];
  }
  else
  {
     for(i=0; i<6; i++)
     {
        if(tempave == temp[i])
        {
          return cap[i];
        }
        else if(tempave < temp[i+1] && (tempave > temp[i]))
        {
           return (cap[i]+(tempave-temp[i])*(cap[i+1]-cap[i])/(temp[i+1]-temp[i]));
        }
     }
  }
}

/*=======================================================================
 *函数名:      MaxVolt_Cal
 *功能:        不同温度下电池的SOC修正时最高电压不同
 *参数:        temp：按电池箱体的平均温度                
 *返回：       float：在此温度下实验得到的电池容量
 *说明：       在不同的温度下SOC终端校正的电压不一样
========================================================================*/ 
/*
float MaxVolt_Cal(uint8 temp)
{
  if(temp>=10+40)
  {
    return 4.3;
  }
  else if(temp>=5+40)
  {
    return 4.2;
  }
  else
  {
    return 3.9;
  }
}
*/
/*=======================================================================
 *函数名:      float inition_soc(float v)
 *功能:        SOC初始化函数，查表取SOC值
 *参数:        无       
 *返回：       无
 *说明：       电压查SOC初值   soc改为float，V改为unsigned short 
========================================================================*/
static
float inition_soc(float v)
{  
  static float soc;
	static uint8 i;              
  #if(CELL_TYPE == 0x03) 
    //磷酸铁锂电池
   //	float socdata[15]={  0 ,   0.1,    0.2,    0.3,    0.4,    0.5,    0.6,    0.7,    0.8,    0.9,   0.92,   0.94,   0.96,   0.98,     1 };	
   //	float uocdata[15]={2.9, 3.2013, 3.2432, 3.2733, 3.2776, 3.2857, 3.2930, 3.3189, 3.3191, 3.3196, 3.3200, 3.3205, 3.3210, 3.3220, 3.3312};
   //楼下100AH磷酸铁锂电池
   static float socdata[11]={     0,    0.1,    0.2,    0.3,    0.4,    0.5,    0.6,    0.7,    0.8,    0.9,     1};
   static float uocdata[11]={3.1400, 3.2150, 3.2520, 3.2780, 3.2920, 3.2930, 3.2950, 3.3060, 3.3320, 3.3340,3.5470};

  	if(v < uocdata[0])
  	{
  	   soc=0 ;
  	} 
  	else if(v>uocdata[10]) 
  	{
  	   soc=1;    	  
  	} 
    else 
  	{ 	  
    	for(i=0;i<11;i++)
    	{  		    
    	 if (v==uocdata[i])
    	  {
    	  	 soc=socdata[i];
    	     break;
    	  } 	
    	  else if ((v<uocdata[i+1]) && (v>uocdata[i])) 
    	  {
  	    	soc=socdata[i]+(v-uocdata[i] )*(socdata[i+1]-socdata[i])/(uocdata[i+1]-uocdata[i]);
        	break;
    	  }	  	  	  
    	} 
  	}  
  #endif 
	return soc;    	
}

/*=======================================================================
 *函数名:      SOC_StaticTime
 *功能:        计算当系统为常电时，系统SOC查表的时间
 *参数:        readTime:实时读取的时间
               current：实时测量电流的大小 
        
 *返回：       uint8：返回是否需要查表,1:进行查表;0:不查表 
 *说明：       当系统为常电时，判断电流为0且SOC在[0~20]或[90~100]范围内时间超过
               3小时时进行查表初始初始化
========================================================================*/
static
uint8 SOC_StaticTime(uint8 readhour, uint8 readminite, float current)
{
    static uint8 Time_firstflag;
    static uint8 cnt[2];
    if(abs(current)<0.5)
    {  
      if(++cnt[0]*SOC_Period/1000>20) //当20S内都是电流小于0.5A开始计时
      {  
        cnt[0] = 20; 
        cnt[1] = 0;
        if(Time_firstflag == 0)
        { 
           Time_firstflag = 1;
           SOCInfo.SOC_CheckTable_StartTime = ((uint16)readhour<<8) | readminite ;           
        }
        else
        {
          if((((uint16)readhour<<8) | readminite)-SOCInfo.SOC_CheckTable_StartTime >= 0)
          {
            if((((uint16)readhour<<8) | readminite)-SOCInfo.SOC_CheckTable_StartTime >= ((uint16)4<<8))//大于4小时查表
            {
                return TRUE;
            }
          }
          else
          {
            if(((uint16)24<<8)-SOCInfo.SOC_CheckTable_StartTime+(((uint16)readhour<<8) | readminite) >= ((uint16)4<<8))
            {   
                return TRUE;
            }
          }
        }
      }
    }
    else if(++cnt[1]*SOC_Period/1000>2)//电流跳变持续超过2s认为它是处于非工作状态
    {
       cnt[0] = 0;
       cnt[1] = 2;
       Time_firstflag = 0;
    }
    return FALSE;
}

/*=======================================================================
 *函数名:      Energy_TotalCal(uint8 mode,float ah1,uint32 Total_V,float Total_E,float current)
 *功能:        计算累计能量
 *参数:        mode:工作状态
               ah1:实时安时
               Total_V:单体电压总和(单位需要转换为V) 
               Total_E:总能量
               current:实时电流 
 *返回：       Total_E:总能量
 
 *说明：       
========================================================================*/ 
static
float Energy_TotalCal(uint8 mode,float ah1,uint32 Total_V,float Total_E,float current)
{
  static float Energy=0;
  static float mode_n;
  
  if(mode == MODE_CHARGE)
  {
    mode_n = (-1);
  }
  else
  {
    mode_n = 1;
  }
  Energy = mode_n*ah1*(((float)Total_V)/10000.0)/1000.0; 
  if(abs(current) > 0.5) 
  {        
    Total_E = Total_E + Energy;//充电总能量KWH
  }
  return (Total_E);
}

/*=======================================================================
 *函数名:      SOH_Cal
 *功能:        SOH的计算
 *参数:        无                
 *返回：       无
 *说明：       按满充的充电次数来计算电池的使用寿命衰减
========================================================================*/  
void SOH_Cal(void) 
{
   uint32 Bms_Life_Count;
     
   Bms_Life_Count = CELL_LIFE_CYCLE*SYS_CAPACITY;  //充电次数(2000)*单体额定容量
   BMSMonitor_SOH.SOH = 1 - (EnergyInfo.Energy_Total_Charge/SYS_VOLT_NOMINAL/1000.0/CELL_LIFE_CYCLE*0.2);   //初始SOH-计算值(标定值：Bms_Life_Count)

}

//=======================================================================
//=============================未配置函数================================
//=======================================================================
   
   //SOH_Cal();              //计算SOH值      
   /*
   if(TimeCount%300 == 0)  //按一定时间存储数据（30s存储速度）
   { 
       TimeCount = 0;
        //BMS运行时间的计算 
       DS3231_Read_Time();  //读取计时(精确到分钟)                                                                          
       EEprom_Data.SOC_Counter_Time = ((uint32)Read_IIC_Time.IIC_Read_Year<<24) +((uint32)Read_IIC_Time.IIC_Read_Month<<16)+ ((uint32)Read_IIC_Time.IIC_Read_Day<<8) + (uint32)(Read_IIC_Time.IIC_Read_Hour);
       
       Time_Interval = Read_IIC_Time.IIC_Read_Minute;       //此时BMS读取的时间(小时)
       if(Time_Interval - SOC_DATA.Start_Run_Time >= 0)
       {
          Value = Time_Interval - SOC_DATA.Start_Run_Time;  //从上电到掉电BMS运行时间间隔；
       }
       else
       {
          Value = 60 - SOC_DATA.Start_Run_Time + Time_Interval;
       }
       SOC_DATA.Start_Run_Time = Time_Interval; 
      
       SOC_DATA.Bms_Run_Time_min = Value + SOC_DATA.Bms_Run_Time_min;  //上电到掉电BMS运行时间间隔的累加
       
       EEprom_write(EEprom_Baseadrr);          
   }*/
       
  


