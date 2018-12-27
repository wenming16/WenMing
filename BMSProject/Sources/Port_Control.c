/*=======================================================================
 *Subsystem:   ���
 *File:        Light.C
 *Author:      WENMING
 *Description: ͨ�ţ�
               �ӿڣ�
               �����ʣ�
 ========================================================================
 * History:    �޸���ʷ��¼�б�
 * 1. Date:
      Author:
      Modification:
========================================================================*/

#include  "Port_Control.h"             //ָʾ����������ͷ�ļ�
//--------------------------------------------------------------------------*
//������: Light_Init                                                        *
//��  ��: ��ʼ��ָʾ��״̬                                                  *
//��  ��: port:�˿���;                                                      * 
//        name:ָ���˿����ź�;                                              *
//        state:��ʼ״̬,1=�ߵ�ƽ,0=�͵�ƽ                                  *
//��  ��: ��                                                                * 
//--------------------------------------------------------------------------*
uint8 Light_Init(uint16 port,uint8 name,uint8 state)   
{
   uint8 Init_state;
   Init_state = Init_GPIO(port,name,1,state);    //��ʼ��ָʾ��
   return(Init_state);
}
 
//--------------------------------------------------------------------------* 
//������: Light_Control                                                     *
//��  ��: ���ƵƵ����Ͱ�                                                    *
//��  ��: port:�˿���;                                                      *
//        name:ָ���˿����ź�;                                              *
//        state:״̬,1=�ߵ�ƽ,0=�͵�ƽ                                      *
//��  ��: ��                                                                * 
//--------------------------------------------------------------------------*   
void Light_Control(uint16 port,uint8 name,uint8 state)
{
    GPIO_Set(port,PRT,name,state);   //��������״̬     
} 

//--------------------------------------------------------------------------* 
//������: Light_Change                                                      *
//��  ��: ״̬�л���ԭ��[��]�����[��]��ԭ��[��]�����[��]                  *
//��  ��: port:�˿���;                                                      *
//        name:ָ���˿����ź�;                                              *
//��  ��: ��                                                                * 
//--------------------------------------------------------------------------*   
void Light_Change(uint16 port,uint8 name)
{
   if(GPIO_Get(port,PRT,name)==Light_ON)   //��ԭ��Ϊ"��",���"��"
     GPIO_Set(port,PRT,name,Light_OFF);
   else                                    //��ԭ��Ϊ"��",���"��"
     GPIO_Set(port,PRT,name,Light_ON);
}

/*=======================================================================
 *������:      Light_StateGet(uint16 port,uint8 pin)
 *����:        light״̬��ȡ
 *����:        port:Portͨ������
               pin:ͨ�����
 *���أ�       state:�̵���״̬
 
 *˵����       
========================================================================*/  
uint8 Light_StateGet(uint16 port,uint8 pin)
{
   uint8 state;
   state = GPIO_Get(port,1,pin);
   return(state);
}



//***********************************�̵���***************************************//
/*=======================================================================
 *������:      Relay_Init(uint16 port,uint8 name,uint8 state)
 *����:        �̵�����ʼ������
 *����:        ��       
 *���أ�       ��
 
 *˵����       
========================================================================*/ 
uint8 Relay_Init(uint16 port,uint8 name,uint8 state) 
{
   uint8 Init_state;
   Init_state = Init_GPIO(port,name,1,state);    //��ʼ���̵��� 
   return(Init_state);
}

/*=======================================================================
 *������:      Relay_Control(uint16 port,uint8 name,uint8 state)
 *����:        �̵������ƺ���
 *����:        ��       
 *���أ�       ��
 
 *˵����       
========================================================================*/ 
void Relay_Control(uint16 port,uint8 name,uint8 state)
{
    GPIO_Set(port,PRT,name,state);   //���Ƽ̵���״̬     
} 

/*=======================================================================
 *������:      Relay_StateGet(uint16 port,uint8 pin)
 *����:        �̵���״̬��ȡ
 *����:        ��       
 *���أ�       ��
 
 *˵����       
========================================================================*/ 
uint8 Relay_StateGet(uint16 port,uint8 pin)
{
   uint8 state;
   state = GPIO_Get(port,1,pin);
   return(state);
}



