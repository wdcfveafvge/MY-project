


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "ohos_init.h"
#include "lwip/sockets.h"
#include "wifi_connect.h"

#include "ohos_init.h"

#include "E53_IA1.h"
#include "oc_mqtt.h"
#include "iot_adc copy.h"
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "hi_time.h"

#define MSGQUEUE_COUNT 16 // number of Message Queue Objects
#define MSGQUEUE_SIZE 10
#define CLOUD_TASK_STACK_SIZE (1024 * 10)
#define CLOUD_TASK_PRIO 24
#define SENSOR_TASK_STACK_SIZE (1024 * 2)
#define SENSOR_TASK_PRIO 25
#define TASK_DELAY_3S 3

#define LED_GPIO 11
#define JIASHI_GPIO 10
#define SHUIBENG_GPIO 6
#define DIANJI_GPIO 8
#define ZHEGUANG1_GPIO 5
#define ZHEGUANG2_GPIO 12
#define duoji1_GPIO 2
#define duoji2_GPIO 10



int duoji1=0,guangzhao=0;

typedef struct { // object data type
    char *Buf;
    uint8_t Idx;
} MSGQUEUE_OBJ_t;

MSGQUEUE_OBJ_t msg;
osMessageQueueId_t mid_MsgQueue; // message queue id

#define CLIENT_ID "6483e2f501554a5933a001c1_8001_0_0_2023061002"
#define USERNAME "6483e2f501554a5933a001c1_8001"
#define PASSWORD "62a49577809132a0bd675a0f2d1c146f6fd0994acb61ab0f069a653626526f48"

typedef enum {
    en_msg_cmd = 0,
    en_msg_report,
} en_msg_type_t;

typedef struct {
    char *request_id;
    char *payload;
} cmd_t;

typedef struct {
    int lum;
    int temp;
    int hum;
} report_t;

typedef struct {
    en_msg_type_t msg_type;
    union {
        cmd_t cmd;
        report_t report;
    } msg;
} app_msg_t;

typedef struct {
    int connected;
    int led;
    int motor;
} app_cb_t;
static app_cb_t g_app_cb;

static void ReportMsg(report_t *report)
{
    oc_mqtt_profile_service_t service;
    oc_mqtt_profile_kv_t temperature;
    oc_mqtt_profile_kv_t humidity;
    oc_mqtt_profile_kv_t luminance;
    oc_mqtt_profile_kv_t led;
    oc_mqtt_profile_kv_t motor;

    service.event_time = NULL;
    service.service_id = "Agriculture";
    service.service_property = &temperature;
    service.nxt = NULL;

    temperature.key = "Temperature";
    temperature.value = &report->temp;
    temperature.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    temperature.nxt = &humidity;

    humidity.key = "Humidity";
    humidity.value = &report->hum;
    humidity.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    humidity.nxt = &luminance;

    luminance.key = "Luminance";
    luminance.value = &report->lum;
    luminance.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    luminance.nxt = &led;

    led.key = "LightStatus";
    led.value = g_app_cb.led ? "ON" : "OFF";
    led.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    led.nxt = &motor;

    motor.key = "MotorStatus";
    motor.value = g_app_cb.motor ? "ON" : "OFF";
    motor.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    motor.nxt = NULL;

    oc_mqtt_profile_propertyreport(USERNAME, &service);
    return;
}

void MsgRcvCallback(uint8_t *recv_data, size_t recv_size, uint8_t **resp_data, size_t *resp_size)
{
    app_msg_t *app_msg;

    int ret = 0;
    app_msg = malloc(sizeof(app_msg_t));
    app_msg->msg_type = en_msg_cmd;
    app_msg->msg.cmd.payload = (char *)recv_data;

    printf("recv data is %s\n", recv_data);
    ret = osMessageQueuePut(mid_MsgQueue, &app_msg, 0U, 0U);
    if (ret != 0) {
        free(recv_data);
    }
    *resp_data = NULL;
    *resp_size = 0;
}

static void oc_cmdresp(cmd_t *cmd, int cmdret)
{
    oc_mqtt_profile_cmdresp_t cmdresp;
    ///< do the response
    cmdresp.paras = NULL;
    cmdresp.request_id = cmd->request_id;
    cmdresp.ret_code = cmdret;
    cmdresp.ret_name = NULL;
    (void)oc_mqtt_profile_cmdresp(NULL, &cmdresp);
}
///< COMMAND DEAL
#include <cJSON.h>
static void DealCmdMsg(cmd_t *cmd)
{
    cJSON *obj_root, *obj_cmdname, *obj_paras, *obj_para;

    int cmdret = 1;

    obj_root = cJSON_Parse(cmd->payload);
    if (obj_root == NULL) {
        oc_cmdresp(cmd, cmdret);
    }

    obj_cmdname = cJSON_GetObjectItem(obj_root, "command_name");
    if (obj_cmdname == NULL) {
        cJSON_Delete(obj_root);
    }
    if (strcmp(cJSON_GetStringValue(obj_cmdname), "Agriculture_Control_light") == 0) {
        obj_paras = cJSON_GetObjectItem(obj_root, "paras");
        if (obj_paras == NULL) {
            cJSON_Delete(obj_root);
        }
        obj_para = cJSON_GetObjectItem(obj_paras, "Light");
        if (obj_para == NULL) {
            cJSON_Delete(obj_root);
        }
        ///< operate the LED here
        if (strcmp(cJSON_GetStringValue(obj_para), "ON") == 0) {
            g_app_cb.led = 1;
            LightStatusSet(ON);
            printf("Light On!");
        } else {
            g_app_cb.led = 0;
            LightStatusSet(OFF);
            printf("Light Off!");
        }
        cmdret = 0;
    } else if (strcmp(cJSON_GetStringValue(obj_cmdname), "Agriculture_Control_Motor") == 0) {
        obj_paras = cJSON_GetObjectItem(obj_root, "Paras");
        if (obj_paras == NULL) {
            cJSON_Delete(obj_root);
        }
        obj_para = cJSON_GetObjectItem(obj_paras, "Motor");
        if (obj_para == NULL) {
            cJSON_Delete(obj_root);
        }
        ///< operate the Motor here
        if (strcmp(cJSON_GetStringValue(obj_para), "ON") == 0) {
            g_app_cb.motor = 1;
            MotorStatusSet(ON);
            printf("Motor On!");
        } else {
            g_app_cb.motor = 0;
            MotorStatusSet(OFF);
            printf("Motor Off!");
        }
        cmdret = 0;
    }

    cJSON_Delete(obj_root);
}

static int CloudMainTaskEntry(void)
{
    app_msg_t *app_msg;

 uint32_t ret = WifiConnect("V-NEW", "wxdzs...");

    device_info_init(CLIENT_ID, USERNAME, PASSWORD);
    oc_mqtt_init();
    oc_set_cmd_rsp_cb(MsgRcvCallback);

    while (1) {
        app_msg = NULL;
        (void)osMessageQueueGet(mid_MsgQueue, (void **)&app_msg, NULL, 0U);
        if (app_msg != NULL) {
            switch (app_msg->msg_type) {
                case en_msg_cmd:
                    DealCmdMsg(&app_msg->msg.cmd);
                    break;
                case en_msg_report:
                    ReportMsg(&app_msg->msg.report);
                    break;
                default:
                    break;
            }
            free(app_msg);
        }
    }
    return 0;
}

void SensorTaskEntry(void)
{
    IoTGpioSetOutputVal(ZHEGUANG1_GPIO, 0);
    IoTGpioSetOutputVal(ZHEGUANG2_GPIO, 0);
 
    unsigned short data1 = 0,sd = 0;
    int ret,ret0 = 0;
    int a,b,c,d; 
    static char line1[32] = {0},line2[32] = {0},line3[32] = {0},line4[32] = {0};
    IoTGpioInit(JIASHI_GPIO);
    IoTGpioInit(SHUIBENG_GPIO);


    IoTGpioInit(LED_GPIO);
    
    IoTGpioSetDir(LED_GPIO, IOT_GPIO_DIR_OUT);
    IoTGpioSetDir(JIASHI_GPIO, IOT_GPIO_DIR_OUT);
    IoTGpioSetDir(SHUIBENG_GPIO, IOT_GPIO_DIR_OUT);


    IoTGpioInit(duoji1_GPIO);
    IoSetFunc(duoji1_GPIO, 0);
    IoTGpioSetDir(duoji1_GPIO, IOT_GPIO_DIR_OUT);
    IoTGpioInit(duoji2_GPIO);
    IoSetFunc(duoji2_GPIO, 0);
    IoTGpioSetDir(duoji2_GPIO, IOT_GPIO_DIR_OUT);
    

    IoTGpioInit(ZHEGUANG1_GPIO);
    IoTGpioInit(ZHEGUANG2_GPIO);
    IoSetFunc(ZHEGUANG2_GPIO, 0);
    IoTGpioSetDir(ZHEGUANG2_GPIO, IOT_GPIO_DIR_OUT);
    IoSetFunc(ZHEGUANG1_GPIO, 0);
    IoTGpioSetDir(ZHEGUANG1_GPIO, IOT_GPIO_DIR_OUT);



    app_msg_t *app_msg;
    E53IA1Data data;
   
    ret = E53IA1Init();
    if (ret != 0) {
        printf("E53_IA1 Init failed!\r\n");
        return;
    }
        ret0 = IoTAdcRead(IOT_ADC_CHANNEL_3, &data1, IOT_ADC_EQU_MODEL_4, IOT_ADC_CUR_BAIS_DEFAULT, 0xff);
        if(data1>1880)
        {
            data1=1880;
        }
         if(data1<1220)
        {
            data1=1220;
        }
         sd=(unsigned short)((1880-(int)data1)/6.6);
   
    while (1) {
        IoTGpioSetOutputVal(ZHEGUANG1_GPIO, 0);
        IoTGpioSetOutputVal(ZHEGUANG2_GPIO, 0);
        ret = E53IA1ReadData(&data);
        if (ret != 0) {
            printf("E53_IA1 Read Data failed!\r\n");
            return;
        }
        app_msg = malloc(sizeof(app_msg_t));
        printf("SENSOR:lum:%.2f temp:%.2f hum:%.2f sd:%d\r\n", data.Lux, data.Temperature, data.Humidity,sd);
        if (app_msg != NULL) {
            app_msg->msg_type = en_msg_report;
            app_msg->msg.report.hum = (int)data.Humidity;
            app_msg->msg.report.lum = (int)data.Lux;
            app_msg->msg.report.temp = (int)data.Temperature;
            if (osMessageQueuePut(mid_MsgQueue, &app_msg, 0U, 0U) != 0) {
                free(app_msg);
            }
        }
         a=(int)data.Lux;b=(int)data.Humidity;c=(int)data.Temperature;
    int ret1 = snprintf(line1, sizeof(line1), "%d", (unsigned short)a);
    int ret2 = snprintf(line2, sizeof(line2), "%d", (unsigned short)b);
    int ret3 = snprintf(line3, sizeof(line3), "%d", (unsigned short)c);
    int ret4 = snprintf(line4, sizeof(line4), "%d", sd);
        oled();//初始化oled
        OledShowString(1, 2,"Lux",1);
        OledShowString(1, 3,"Humidity",1);
        OledShowString(1, 4,"Temperature",1); 
        OledShowString(1, 5,"turangshidu",1); 
        OledShowString(100, 2, line1,1);
        OledShowString(100, 3, line2,1);
        OledShowString(100, 4, line3,1);
        OledShowString(100, 5, line4,1);
         if ((int)data.Lux < 100) {
        IoTGpioSetOutputVal(LED_GPIO, 1); // 设置GPIO_6输出高电平打开电机
        } else {
         IoTGpioSetOutputVal(LED_GPIO, 0); // 设置GPIO_6输出高电平打开电机
        }
          if ((int)data.Lux > 200&&guangzhao==0) {

        IoTGpioSetOutputVal(ZHEGUANG1_GPIO, 0);
        IoTGpioSetOutputVal(ZHEGUANG2_GPIO, 1);
        TaskMsleep(1000);
        IoTGpioSetOutputVal(ZHEGUANG1_GPIO, 0);
        IoTGpioSetOutputVal(ZHEGUANG2_GPIO, 0);

                guangzhao=1;

        } 
          if ((int)data.Lux < 200&&guangzhao==1) {
        IoTGpioSetOutputVal(ZHEGUANG1_GPIO, 1);
        IoTGpioSetOutputVal(ZHEGUANG2_GPIO, 0);
        TaskMsleep(1000);
        IoTGpioSetOutputVal(ZHEGUANG1_GPIO, 0);
        IoTGpioSetOutputVal(ZHEGUANG2_GPIO, 0);           
                guangzhao=0;
        } 

        if (((int)data.Humidity > 70) | ((int)data.Temperature > 35)) {
        MotorStatusSet(ON); // 设置GPIO_8输出高电平打开电机
        } else {
        MotorStatusSet(OFF); // 设置GPIO_8输出低电平关闭电机
        }
         if((int)data.Temperature > 30&&duoji1==0)
        {

                  /*
         * 舵机右转90度*/
        EngineTurnRight(500);
         EngineTurnRight1(2500);
        duoji1=1;

        }
         if((int)data.Temperature < 30&&duoji1==1)
        {
       RegressMiddle(1500);
       RegressMiddle1(1500);
        duoji1=0;

        }

        if((int)sd<50)
          {
            IoTGpioSetOutputVal(SHUIBENG_GPIO, 1); // 设置GPIO_6输出高电平打开电机
           }
           else
           {
            IoTGpioSetOutputVal(SHUIBENG_GPIO, 0); // 设置GPIO_6输出低电平关闭电机
           }
        sleep(TASK_DELAY_3S);
    }
    return 0;
}

static void IotMainTaskEntry(void)
{
    mid_MsgQueue = osMessageQueueNew(MSGQUEUE_COUNT, MSGQUEUE_SIZE, NULL);
    if (mid_MsgQueue == NULL) {
        printf("Failed to create Message Queue!\n");
    }

    osThreadAttr_t attr;

    attr.name = "CloudMainTaskEntry";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = CLOUD_TASK_STACK_SIZE;
    attr.priority = CLOUD_TASK_PRIO;

    if (osThreadNew((osThreadFunc_t)CloudMainTaskEntry, NULL, &attr) == NULL) {
        printf("Failed to create CloudMainTaskEntry!\n");
    }
    attr.stack_size = SENSOR_TASK_STACK_SIZE;
    attr.priority = SENSOR_TASK_PRIO;
    attr.name = "SensorTaskEntry";
    if (osThreadNew((osThreadFunc_t)SensorTaskEntry, NULL, &attr) == NULL) {
        printf("Failed to create SensorTaskEntry!\n");
    }
    
}

APP_FEATURE_INIT(IotMainTaskEntry);