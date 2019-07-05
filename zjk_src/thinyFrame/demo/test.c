#include <string.h>
#include "TinyFrame.h"
#include "utils.h"
#include "cmsis_os.h"
//zjk include
#include "z_serial.h"
#include "z_param.h"

#include "z_include.h"
TinyFrame *demo_tf=NULL;
TinyFrame zjk_tf;	

bool ifDebug=false;  //是否调试标记
bool do_corrupt = false;
extern const char *romeo;
bool isFlashParam;   //是否保存数据
void z_ListenerInit(void);
extern TaskHandle_t xHandleGp21Trig;
typedef bool z_lkParmStatuType;

lk_statu_ lk_param_statu={
  .ifParamSave =   false,
	.ifParamGet =    false,
	.ifGetOnceDist = false,
	.ifContinuDist = false,
	.ifStopContinu = false,
	.ifQCStand = false,
	.ifQCgetParm = false	
};


void parmSend(parm_ *parm);
/*字节数组转换对应的结构体*/
parm_* byteToStruct(uint8_t *buff)
{
  parm_* parm = (parm_ *)buff;
	
  return parm;
}

/*结构体转换对应字节*/
arrayByte_ structToBytes(parm_ *p)
{
	  arrayByte_ arraybuff;
    arraybuff.point =  (uint8_t*)(p);
	  arraybuff.lens = sizeof(* p);
	  return arraybuff;
} 

/*激光器保存参数*/
parm_ lk_defaultParm ={
	.product = 0x02,     //产品号lk03
	.baud_rate = 115200, //波特率
	.limit_trigger = 100, //100米触发
	.red_laser_light = 0x01, //打开:0x01 关闭：0
  .front_or_base = 0,      //前基准：1 后基准：0
	.ifHasConfig = 0,      //第一次烧写flash后会变成0x01
	.outFreq = 100
};



/*激光器保存参数*/
//parm_ lk_parm ={
//	.product = 0,     //产品号lk03
//	.baud_rate = 0, //波特率
//	.limit_trigger = 0, //100米触发
//	.red_laser_light = 0, //打开:0x01 关闭：0
//  .front_or_base = 0,      //前基准：1 后基准：0
//};
parm_ lk_flash=
 { 
		.product = 0,     //产品号lk03
		.baud_rate = 0, //波特率
		.limit_trigger = 0, //100米触发
		.red_laser_light = 0, //打开:0x01 关闭：0
		.front_or_base = 0,      //前基准：1 后基准：0
	 .ifHasConfig = 0,      //第一次烧写flash后会变成0x01
};



const uint8_t distance_setCmd[DistStop][2]=
{
  {DataDistSend,DistOnce},{DataDistSend,DistContinue},{DataDistSend,DistStop},//距离
};

const uint8_t param_getCmd[ParamAll][2]=
{
 {ParmsConfig,ParamAll},   //参数获取
};

const uint8_t param_configCmd[AutoMel][2]=
{
	{ParmaSend,BarudRate},{ParmaSend,RedLight},{ParmaSend,FrontOrBase},{ParmaSend,AutoMel},//参数配置
};
const uint8_t qc_Cmd[GetParam][2]=  //QC命令
{
  {QC,standStart},{QC,StandParamFirst},{QC,StandParamSecond},{QC,StandParamThird},{QC,StandParamFirstReset},{QC,StandParamSecondReset},{QC,StandParamThirdReset},{QC,GetParam},//QC命令
};

/*数据获取命令*/
void dataGetCmdSlect(FRAME_GetDataID_CMD  DATA_GET, TF_Msg *msg)
{

	switch(DATA_GET)
	{
		case DistOnce :    //单次测量命令
		{
			 lk_param_statu.ifGetOnceDist = true;
		}break;
	  case DistContinue:  //连续测量命令
		{
			  lk_param_statu.ifContinuDist = true;
		}break;
		case DistStop:   //停止测量
		{
		    stop_txSignl_Tim();    //停止发射信号
			  lk_param_statu.ifGetOnceDist = false;		
        lk_param_statu.ifContinuDist = false;
				lk_param_statu.ifStopContinu = false;		
		}break;
	}
	 if((lk_param_statu.ifGetOnceDist) || (lk_param_statu.ifContinuDist))
	 {
	    // vTaskResume(xHandleGp21Trig);   //恢复任务状态
	 }

}
/*参数获取*/
void paramGetCmdSlect(FRAME_GetParam_CMD Param_GET, TF_Msg *msg)
{
  
   switch(Param_GET)  //参数获取
	 {
		 case ParamAll:
		 {
		   lk_param_statu.ifParamGet= true; 
		 }break;
	 
	 }
}


/*参数保存命令*/
void paramDataSaveCMD(FRAME_ParmSaveID_CMD PAMRM_SAVE, TF_Msg *msg)
{

	switch(PAMRM_SAVE)
	{
		case BarudRate:
		{
        lk_defaultParm.baud_rate = *(int*)(msg->data);
		}break;
		
		case RedLight:
		{
          lk_defaultParm.red_laser_light = *(uint8_t *)(msg->data);
		}break;
		
		case FrontOrBase:
		{
         lk_defaultParm.front_or_base =  *(uint8_t *)(msg->data);
		}break;
		case AutoMel:  //自动测量
		{
         
		}break;		
	}
  lk_param_statu.ifParamSave =true;
}


/*QC 检测命令*/
void QC_CMD(FRAME_ParmQC_CMD qc, TF_Msg *msg)
{
   FRAME_ParmQC_CMD cnd=qc;
	switch(cnd)
	{
		case standStart:
		{
       
		}break;
		case StandParamFirst:  //上位机第1档标定值
		{
        lk_defaultParm.QC[LK03_FIRST_STAND].qc_stand_dist= msg->data[0]<<8|msg->data[1];
			  lk_defaultParm.QC[LK03_FIRST_STAND].qc_ad603Gain= msg->data[2]<<8|msg->data[3];
			  lk_param_statu.ifQCStand[LK03_FIRST_STAND] =true;
		}break;
		case StandParamSecond:  //上位机第2档标定值
		{
        lk_defaultParm.QC[LK03_SECOND_STAND].qc_stand_dist= msg->data[0]<<8|msg->data[1];
			  lk_defaultParm.QC[LK03_SECOND_STAND].qc_ad603Gain= msg->data[2]<<8|msg->data[3];
			  lk_param_statu.ifQCStand[LK03_SECOND_STAND] =true;
		}break;	
		case StandParamThird:  //上位机第3档标定值
		{
        lk_defaultParm.QC[LK03_THIRD_STAND].qc_stand_dist= msg->data[0]<<8|msg->data[1];
			  lk_defaultParm.QC[LK03_THIRD_STAND].qc_ad603Gain= msg->data[2]<<8|msg->data[3];
			  lk_param_statu.ifQCStand[LK03_THIRD_STAND] =true;
		}break;	
		case StandParamFirstReset:  //第1档从新校准
		{
			lk_param_statu.ifQCgetParmReset[LK03_FIRST_STAND] = true;
		}break;
		case StandParamSecondReset:  //第2档从新校准
		{
		 	lk_param_statu.ifQCgetParmReset[LK03_SECOND_STAND] = true;
		}break;	
		case StandParamThirdReset:  //第3档从新校准
		{
			lk_param_statu.ifQCgetParmReset[LK03_THIRD_STAND] = true;
		}break;	
		case StandFirstSwitch:  //档位1切换
		{
			lk_param_statu.ifstandSwitch[LK03_FIRST_STAND] = true;
		}break;
		case StandSecondSwitch:  //档位2切换
		{
		 	lk_param_statu.ifstandSwitch[LK03_SECOND_STAND] = true;
		}break;	
		case StandThirdSwitch:  //档位3切换
		{
			lk_param_statu.ifstandSwitch[LK03_THIRD_STAND] = true;
		}break;			
    case GetParam :  //获取参数
		{
			  lk_param_statu.ifQCgetParm = true;
		
		}break;			
	}
 
}


//调试命令
void debug_cmd(FRAME_DEBUG_CMD cmd,TF_Msg *msg)
{
   FRAME_DEBUG_CMD cmd_id=cmd;
   switch(cmd_id) 
	 {
	   case debug_ID:
		 {
			  if(msg->data[0]==1)
				{
				  ifDebug = true;
				}
			  if(msg->data[0]==0)
				{
				  ifDebug = false;
				}		 	  
		 }break;
	 
	 }

}
/**
 * This function should be defined in the application code.
 * It implements the lowest layer - sending bytes to UART (or other)
 */
void TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len)
{		
		//zjk
		z_serial_write((uint8_t*)buff,len);
}

/*通用监听回调函数*/
TF_Msg *cmdMsg =NULL;
 TF_Result myGenericListener(TinyFrame *tf, TF_Msg *msg)
{
 
	cmdMsg = msg;
	FRAME_TYPE_CMD typeCMD = (FRAME_TYPE_CMD) (cmdMsg->type);
	 switch(typeCMD)
	{
	  case DataDistSend:/*测量命令*/
		{
			FRAME_GetDataID_CMD getDataCmd =  (FRAME_GetDataID_CMD) (cmdMsg->frame_id);
		  dataGetCmdSlect(getDataCmd,msg);
		}break;
	  case ParmaSend: /*参数获取*/
		{
			FRAME_GetParam_CMD getParamCmd =  (FRAME_GetParam_CMD) (cmdMsg->frame_id);
		  paramGetCmdSlect(getParamCmd,msg);
		}break;		
	  case ParmsConfig: /*参数配置*/
		{
		  FRAME_ParmSaveID_CMD parmaSaveCmd = (FRAME_ParmSaveID_CMD) (cmdMsg->frame_id);
		  paramDataSaveCMD(parmaSaveCmd,msg);
		}	break;
		case QC:   /*标定命令*/
		{
		  FRAME_ParmQC_CMD QCCmd = (FRAME_ParmQC_CMD) (cmdMsg->frame_id);
		  QC_CMD(QCCmd,msg);
		}break;
		case lk_debug:   /*调试命令*/
		{
		  FRAME_DEBUG_CMD debugCmd = (FRAME_DEBUG_CMD) (cmdMsg->frame_id);
		  debug_cmd(debugCmd,msg);
		}break;				
		case ErroSend:
		{
		  
		}break;
		
	}
	
    return TF_STAY;
}


void tinyRecFunc(uint8_t *buf)
{
		 uint16_t lens =0;
     uint8_t testbuf[50] = {0};
		 get_revLens(&lens);
		 for(int i=0;i<lens;i++)
		{
		  testbuf[i] = buf[i];
		
		}
	   TF_Accept(demo_tf, buf, lens);
	
	}


void parmSend(parm_ *parm)
{
    TF_Msg msg;
    TF_ClearMsg(&msg);	
 	  arrayByte_ arrayBuff; 
	  arrayBuff = structToBytes(parm); 
  	msg.type = ParmaSend;
	  msg.frame_id = ParamAll;
	  msg.data = arrayBuff.point;
    msg.len = arrayBuff.lens;
  	TF_Respond(demo_tf, &msg);	
}
//qc标定参数发送
void QCparmSend(uint8_t *data,uint8_t lens)
{
    TF_Msg msg;
    TF_ClearMsg(&msg);	
  	msg.type = QC;
	  msg.frame_id = GetParam;
	  msg.data = data;
    msg.len = lens;
  	TF_Respond(demo_tf, &msg);	
	
}

//调试debug命令测试
void debugParmSend(uint8_t *data,uint8_t lens)
{
    TF_Msg msg;
    TF_ClearMsg(&msg);	
  	msg.type = lk_debug;
	  msg.frame_id = debug_ID;
	  msg.data = data;
    msg.len = lens;
  	TF_Respond(demo_tf, &msg);	
}


void z_tiny_test(void)
{
   z_ListenerInit();
	 //parmSend(&lk_defaultParm);
	 if(addUartDmaRevListen(tinyRecFunc)) 
	 {	 
		 
	 }
	 else
	 {
	    //add false
	 }
 
}

void zTF_sendOnceDist(uint8_t *data,uint8_t lens)
{
    TF_Msg msg;
    TF_ClearMsg(&msg);
    msg.type = DataDistSend;
	  msg.frame_id = DistOnce;
    msg.data = data;
    msg.len = lens;
  //  TF_Send(demo_tf, &msg);
  	TF_Respond(demo_tf, &msg);
}

/*id listener add*/
void z_ListenerInit(void)
{
    demo_tf = &zjk_tf;
    TF_InitStatic(demo_tf, TF_MASTER);
    TF_AddGenericListener(demo_tf, myGenericListener); 
}
