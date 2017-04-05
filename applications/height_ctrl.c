/******************** (C) COPYRIGHT 2016 ANO Tech ********************************
  * ����   �������ƴ�
 * �ļ���  ��height_ctrl.c
 * ����    ���߶ȿ���
 * ����    ��www.anotc.com
 * �Ա�    ��anotc.taobao.com
 * ����QȺ ��190169595
**********************************************************************************/
#include "height_ctrl.h"
#include "anotc_baro_ctrl.h"
#include "mymath.h"
#include "filter.h"
#include "rc.h"
#include "PID.h"
#include "ctrl.h"
#include "include.h"
#include "fly_mode.h"

float	set_height_e,
		set_height_em,
		set_speed_t,	//ң��������ת��Ϊ�����ٶ�ʱ�õ��м������������ͨ�˲��õ�set_speed
		set_speed,		//ң�������õ������ٶȣ���λmm/s
		exp_speed,		//λ��PID����������ٶȣ������ٶ�PID
		fb_speed,
		exp_acc,fb_acc,fb_speed,fb_speed_old;

_hc_value_st hc_value;


u8 thr_take_off_f = 0;


_PID_arg_st h_acc_arg;		//���ٶ�
_PID_arg_st h_speed_arg;	//�ٶ�
_PID_arg_st h_height_arg;	//�߶�

_PID_val_st h_acc_val;
_PID_val_st h_speed_val;
_PID_val_st h_height_val;

void h_pid_init()
{
	h_acc_arg.kp = 0.01f ;				//����ϵ��
	h_acc_arg.ki = 0.02f  *pid_setup.groups.hc_sp.kp;				//����ϵ��
	h_acc_arg.kd = 0;				//΢��ϵ��
	h_acc_arg.k_pre_d = 0 ;	
	h_acc_arg.inc_hz = 0;
	h_acc_arg.k_inc_d_norm = 0.0f;
	h_acc_arg.k_ff = 0.05f;

	h_speed_arg.kp = 1.5f *pid_setup.groups.hc_sp.kp;				//����ϵ��
	h_speed_arg.ki = 0.0f *pid_setup.groups.hc_sp.ki;				//����ϵ��
	h_speed_arg.kd = 0.0f;				//΢��ϵ��
	h_speed_arg.k_pre_d = 0.10f *pid_setup.groups.hc_sp.kd;
	h_speed_arg.inc_hz = 20;
	h_speed_arg.k_inc_d_norm = 0.8f;
	h_speed_arg.k_ff = 0.5f;	
	
	h_height_arg.kp = 1.5f *pid_setup.groups.hc_height.kp;				//����ϵ��
	h_height_arg.ki = 0.0f *pid_setup.groups.hc_height.ki;				//����ϵ��
	h_height_arg.kd = 0.05f *pid_setup.groups.hc_height.kd;				//΢��ϵ��
	h_height_arg.k_pre_d = 0.01f ;
	h_height_arg.inc_hz = 20;
	h_height_arg.k_inc_d_norm = 0.5f;
	h_height_arg.k_ff = 0;	
	
}

float thr_set,thr_pid_out,thr_out,thr_take_off,tilted_fix;

float en_old;
u8 ex_i_en;

float Height_Ctrl(float T,float thr,u8 ready,float en)	//en	1������   0���Ƕ���
{
	/*
	 * �����㷨�и�������˼�ĵط���
	 * ���ٶ�PID�õ��������ٶ�ֵ���ϸ������ٶ�PID������ģ�
	 * �ٶ�PID�õ������ٶ�ֵ���ϸ�����λ��PID�������
	 * ��������������ʵʱ����ʵ�Ƿǳ����õģ��ٶ�--���ٶ��ӳ�20ms��λ��--�ٶ��ӳ�20ms�����������ӳ���40ms
	 * 
	 */
	
	//thr��0 -- 1000
	static u8 speed_cnt,height_cnt;
	
	//�߶����ݻ�ȡ����ȡ��ѹ�����ݣ����ó��������ݣ��ںϼ���߶����ݣ�
	baro_ctrl(T,&hc_value); 
	
	//��������жϣ�����ѡ����������Ƿ�ִ��
	if(ready == 0)	//û�н������Ѿ�������
	{
		en = 0;						//ת��Ϊ�ֶ�ģʽ����ֹ�Զ����ߴ����ִ��
		thr_take_off_f = 0;			//��ɱ�־����
		thr_take_off = 0;			//��׼��������
	}
	
	//���Ŵ���
	//ȡֵ��Χת������������
	thr_set = my_deathzoom_2(my_deathzoom((thr - 500),0,40),0,10);	//��50Ϊ���������Ϊ��40��λ��
	
	//thr_set�Ǿ����������õ����ſ���������ֵ��ȡֵ��Χ -500 -- +500
	
	//ģʽ�ж�
	if(en < 0.1f)		//�ֶ�ģʽ
	{
		en_old = en;	//������ʷģʽ
		
		return (thr);	//thr�Ǵ��������ֵ��thr��0 -- 1000
						//�Ѵ�������ֱ�Ӵ���ȥ�ˣ����������㷨��û����
	}
	else
	{
		//���ģʽ�л�
		/*�����г��ν��붨��ģʽ�л�����*/
		if( ABS(en - en_old) > 0.5f )	//�ӷǶ����л������ߣ��ٷ�ע�ͣ�	//����Ϊ��ģʽ�ڷ����б��л����л�����ȷ��
		{
			
			if(thr_take_off<10)			//δ����������ţ��ٷ�ע�ͣ�
			{
				//thr_set�Ǿ����������õ����ſ���������ֵ��ȡֵ��Χ -500 -- +500��
				//����ж��п�������ڵ����Ͻ�������һ������ʱ���л�ģʽ���·ɻ�һ���Ӵ����죨�ȶ�����ɻ��죩
				if(thr_set > -150)		//thr_set > -150 �������ŷǵ�
				{
					thr_take_off = 400;
				}
			}
			en_old = en;	//������ʷģʽ
		}
		
		//�����жϣ������ٶ�����
		if(thr_set>0)	//����
		{
			set_speed_t = thr_set/450 * MAX_VERTICAL_SPEED_UP;	//set_speed_t ��ʾ���������ٶ�ռ��������ٶȵı�ֵ
			
			//��ɴ���
			if(thr_take_off_f == 0)	//���û����ɣ����ν�����û����ɣ�
			{
				if(thr_set>100)	//�ﵽ�������
				{
					thr_take_off_f = 1;	//��ɱ�־��1���˱�־ֻ��������ᱻ����
					thr_take_off = 350; //ֱ�Ӹ�ֵ��ɻ�׼����
				}
			}
		}
		else			//��ͣ���½�
		{
			set_speed_t = thr_set/450 * MAX_VERTICAL_SPEED_DW;	//set_speed_t ��ʾ���������ٶ�ռ����½��ٶȵı�ֵ
		}
		set_speed_t = LIMIT(set_speed_t,-MAX_VERTICAL_SPEED_DW,MAX_VERTICAL_SPEED_UP);	//�ٶ������޷�
		LPF_1_(10.0f,T,my_pow_2_curve(set_speed_t,0.25f,MAX_VERTICAL_SPEED_DW),set_speed);	//LPF_1_�ǵ�ͨ�˲���������Ƶ����10Hz�����ֵ��set_speed
																							//my_pow_2_curve����������ת��Ϊ2�׵����ߣ���0����ƽ��������ֵ�ϴ�Ĳ���ж�ʴ�
		set_speed = LIMIT(set_speed,-MAX_VERTICAL_SPEED_DW,MAX_VERTICAL_SPEED_UP);	//�޷�����λmm/s
		
		
		//����߶����ɼ��˲���
		//�߶Ȳ� = ���ٶȲ�*T ����λ mm/s��
		//h(n) = h(n-1) + ��h  �� ��h =�������ٶ� - ��ǰ�ٶȣ� * ��t
		//�ý���״̬��Ŀ��߶Ȳ���ֿ��Ʊ�����ֵ��ֻ����ex_i_en = 1ʱ�ŻῪʼ���ּ���Ŀ��߶Ȳ�
		ex_i_en = thr_take_off_f;
		
		set_height_em += (set_speed -        hc_value.m_speed)      * T;	//û�о������ٶ������ʹ�ͨ�˲����ٶ�ֵ������ٶȲ� * ��T
		set_height_em = LIMIT(set_height_em,-5000 *ex_i_en,5000 *ex_i_en);	//ex_i_en = 1 ��ʾ�Ѿ�����������ţ�����Ϊ0
		
		set_height_e += (set_speed  - 1.05f *hc_value.fusion_speed) * T;	//�������ٶ������ʹ�ͨ�˲����ٶ�ֵ������ٶȲ� * ��T
		set_height_e  = LIMIT(set_height_e ,-5000 *ex_i_en,5000 *ex_i_en);
		
		LPF_1_(0.05f,T,set_height_em,set_height_e);	//Ƶ�� ʱ�� ���� ���	//�����ٶȲ�����ںϣ���һ������Խ��set_height_em��ռ��Խ��	
		
		//���˵ó��߶Ȳ� set_height_e ����λ mm
		
		//===============================================================================
		//	���ٶ�PID
			
		float acc_i_lim;
		acc_i_lim = safe_div(150,h_acc_arg.ki,0);		//acc_i_lim = 150 / h_acc_arg.ki
														//����������������ֳ���������͵�0��
		
		//������ٶ�
		fb_speed_old = fb_speed;						//�洢��һ�ε��ٶ�
		fb_speed = hc_value.fusion_speed;				//��ȡ��ǰ�ٶ�
		fb_acc = safe_div(fb_speed - fb_speed_old,T,0);	//����õ����ٶȣ�a = dy/dt = [ x(n)-x(n-1)]/dt
		
		//fb_acc�ǵ�ǰ���ٶ�ֵ�����������ļ��ٶ�ֵ��
		
		//���ٶ�PID
		thr_pid_out = PID_calculate( T,            		//����
									 exp_acc,			//ǰ��				//exp_acc���ٶ�PID����
									 exp_acc,			//����ֵ���趨ֵ��	//exp_acc���ٶ�PID����
									 fb_acc,			//����ֵ
									 &h_acc_arg, 		//PID�����ṹ��
									 &h_acc_val,		//PID���ݽṹ��
									 acc_i_lim*en		//integration limit�������޷�     ������ֶ�ģʽ��en = 0������������0��
									);					//���
									
		
		//��׼���ŵ�������ֹ���ֱ��͹��
		if(h_acc_val.err_i > (acc_i_lim * 0.2f))
		{
			if(thr_take_off<THR_TAKE_OFF_LIMIT)
			{
				thr_take_off += 150 *T;
				h_acc_val.err_i -= safe_div(150,h_acc_arg.ki,0) *T;
			}
		}
		else if(h_acc_val.err_i < (-acc_i_lim * 0.2f))
		{
			if(thr_take_off>0)
			{
				thr_take_off -= 150 *T;
				h_acc_val.err_i += safe_div(150,h_acc_arg.ki,0) *T;
			}
		}
		thr_take_off = LIMIT(thr_take_off,0,THR_TAKE_OFF_LIMIT); //�޷�
		
		/////////////////////////////////////////////////////////////////////////////////
		
		//���Ų������������
		tilted_fix = safe_div(1,LIMIT(reference_v.z,0.707f,1),0); //45���ڲ���
		thr_out = (thr_pid_out + tilted_fix *(thr_take_off) );	//����������ɣ�����PID + ���Ų��� * �������
		thr_out = LIMIT(thr_out,0,1000);
		
		
		/////////////////////////////////////////////////////////////////////////////////	
		
		//�ٶ�PID
		
		static float dT,dT2;
		dT += T;
		speed_cnt++;
		if(speed_cnt>=10) //u8  20ms
		{
			speed_cnt = 0;
			
			exp_acc = PID_calculate( dT,           				//����
									exp_speed,					//ǰ��				//exp_speed��λ��PID����
									(set_speed + exp_speed),	//����ֵ���趨ֵ��	//set_speed���������������exp_speed��λ��PID����
									hc_value.fusion_speed,		//����ֵ
									&h_speed_arg, 				//PID�����ṹ��
									&h_speed_val,				//PID���ݽṹ��
									500 *en						//integration limit�������޷�
									 );							//���	
			
			exp_acc = LIMIT(exp_acc,-3000,3000);
			dT = 0;
			
			//��δ��������ڼ���Ŀ��߶Ȳ�
			//integra_fix += (exp_speed - hc_value.m_speed) *dT;
			//integra_fix = LIMIT(integra_fix,-1500 *en,1500 *en);
			//LPF_1_(0.5f,dT,integra_fix,h_speed_val.err_i);
			
			dT2 += dT;		//����΢��ʱ��
			height_cnt++;	//����ѭ��ִ������
			if(height_cnt>=10)  //200ms 
			{
				height_cnt = 0;
				
				//λ��PID
				//����ķ���ֵ�Ǹ߶Ȳ�����Ǹ߶ȣ��൱���Ѿ���error�����ˣ���������ֵΪ0ʱ������ error = error - 0
				exp_speed = PID_calculate( 		dT2,            //����
												0,				//ǰ��
												0,				//����ֵ���趨ֵ��
												-set_height_e,	//����ֵ				�߶Ȳ��λmm
												&h_height_arg, 	//PID�����ṹ��
												&h_height_val,	//PID���ݽṹ��
												1500 *en		//integration limit�������޷�
										 );			//���	
				exp_speed = LIMIT(exp_speed,-300,300);
				
				dT2 = 0;
			}
		}
		
		return (thr_out);	//�����������������ֵ
	}

}

/******************* (C) COPYRIGHT 2016 ANO TECH *****END OF FILE************/

//u8 auto_take_off,auto_land;
//float height_ref;

//float auto_take_off_land(float dT,u8 ready)
//{
//	static u8 back_home_old;
//	static float thr_auto;
//	
//	if(ready==0)	//�����ж�
//	{
//		height_ref = hc_value.fusion_height;
//		auto_take_off = 0;
//	}
//	
//	if(Thr_Low == 1 && fly_ready == 0)
//	{
//		if(mode_value[BACK_HOME] == 1 && back_home_old == 0) //���֮ǰ�����ҽ���֮ǰ���Ƿ���ģʽ��������ģʽ
//		{
//				if(auto_take_off==0)  //��һ�����Զ���ɱ��0->1
//				{
//					auto_take_off = 1;
//				}
//		}
//	}
//	
//	switch(auto_take_off)
//	{
//		case 1:
//		{
//			if(thr_take_off_f ==1)
//			{
//				auto_take_off = 2;
//			}
//			break;
//		}
//		case 2:
//		{
//			if(hc_value.fusion_height - height_ref>500)
//			{
//				if(auto_take_off==2) //�Ѿ������Զ����
//				{
//					auto_take_off = 3;
//				}
//			}
//		
//		}
//		default:break;
//	}

//	

//	
//	if(auto_take_off == 2)
//	{
//		thr_auto = 200;
//	}
//	else if(auto_take_off == 3)
//	{
//		thr_auto -= 200 *dT;
//	
//	}
//	
//	thr_auto = LIMIT(thr_auto,0,300);
//	
//	back_home_old = mode_value[BACK_HOME]; //��¼ģʽ��ʷ
//		
//	return (thr_auto);
//}

//	�ɵ�����жϴ���
//	if(thr_set>0)	//����
//	{
//		set_speed_t = thr_set/450 * MAX_VERTICAL_SPEED_UP;	//set_speed_t ��ʾ���������ٶ�ռ��������ٶȵı�ֵ
//		
//		if(thr_set>100)	//�ﵽ�������
//		{
//			ex_i_en_f = 1;
//			
//			if(!thr_take_off_f)	//���û����ɣ����ν�����û����ɣ�
//			{
//				thr_take_off_f = 1; //�û�������Ҫ��ɣ��л�Ϊ�Ѿ���ɣ�
//				thr_take_off = 350; //ֱ�Ӹ�ֵ һ��
//			}
//		}
//	}
//	else			//��ͣ���½�
//	{
//		if(ex_i_en_f == 1)	//���ϵ翪ʼ���ֹ�������ţ���һ�ν���ǰ���ű����������ͣ����԰ѳ��ν���ǰ�����Ÿ˲�����Ӱ��
//		{
//			ex_i_en = 1;	//��ʾ���������������ţ��Ѿ���ɻ��ϵ����������
//		}
//		set_speed_t = thr_set/450 * MAX_VERTICAL_SPEED_DW;	//set_speed_t ��ʾ���������ٶ�ռ����½��ٶȵı�ֵ
//	}
