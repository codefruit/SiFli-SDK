/**
  ******************************************************************************
  * @file   ft6336.c
  * @author Sifli software development team
  ******************************************************************************
*/
/**
 * @attention
 * Copyright (c) 2019 - 2022,  Sifli Technology
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Sifli integrated circuit
 *    in a product or a software update for such product, must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Sifli nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Sifli integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY SIFLI TECHNOLOGY "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SIFLI TECHNOLOGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <rtthread.h>
#include "board.h"
#include "ft6336.h"
#include "drv_touch.h"

/* Define -------------------------------------------------------------------*/

#define DBG_LEVEL          DBG_ERROR  // DBG_LOG //
#define LOG_TAG              "drv.ft6336"
#include <drv_log.h>

#define FT_DEV_ADDR             (0x38)
#define FT_TD_STATUS            (0x02)
#define FT_P1_XH                (0x03)
#define FT_P1_XL                (0x04)
#define FT_P1_YH                (0x05)
#define FT_P1_YL                (0x06)
#define FT_ID_G_MODE            (0xA4)



#define FT_MAX_WIDTH                   (240)
#define FT_MAX_HEIGHT                  (240)

// rotate to left with 90, 180, 270
// rotate to left with 360 for mirror
//#define FT_ROTATE_LEFT                 (90)

/* function and value-----------------------------------------------------------*/


static void ft6336_correct_pos(touch_msg_t ppos);
static rt_err_t write_reg(uint8_t reg, rt_uint8_t data);
static rt_err_t read_regs(rt_uint8_t reg, rt_uint8_t len, rt_uint8_t *buf);

static struct rt_i2c_bus_device *ft_bus = NULL;

static struct touch_drivers ft6336_driver;


static rt_err_t write_reg(uint8_t reg, rt_uint8_t data)
{
    rt_int8_t res = 0;
    struct rt_i2c_msg msgs;
    rt_uint8_t buf[2] = {reg, data};

    msgs.addr  = FT_DEV_ADDR;    /* slave address */
    msgs.flags = RT_I2C_WR;        /* write flag */
    msgs.buf   = buf;              /* Send data pointer */
    msgs.len   = 2;

    if (rt_i2c_transfer(ft_bus, &msgs, 1) == 1)
    {
        res = RT_EOK;
    }
    else
    {
        res = -RT_ERROR;
    }
    return res;
}

static rt_err_t read_regs(rt_uint8_t reg, rt_uint8_t len, rt_uint8_t *buf)
{
    rt_int8_t res = 0;
    struct rt_i2c_msg msgs[2];

    msgs[0].addr  = FT_DEV_ADDR;    /* Slave address */
    msgs[0].flags = RT_I2C_WR;        /* Write flag */
    msgs[0].buf   = &reg;             /* Slave register address */
    msgs[0].len   = 1;                /* Number of bytes sent */

    msgs[1].addr  = FT_DEV_ADDR;    /* Slave address */
    msgs[1].flags = RT_I2C_RD;        /* Read flag */
    msgs[1].buf   = buf;              /* Read data pointer */
    msgs[1].len   = len;              /* Number of bytes read */

    if (rt_i2c_transfer(ft_bus, msgs, 2) == 2)
    {
        res = RT_EOK;
    }
    else
    {
        res = -RT_ERROR;
    }
    return res;
}



static void ft6336_correct_pos(touch_msg_t ppos)
{
    ppos->x = FT_MAX_WIDTH - ppos->x;
    if (ppos->x < 0)
    {
        ppos->x = 0;
    }

    ppos->y = FT_MAX_HEIGHT - ppos->y;
    if (ppos->y < 0)
    {
        ppos->y = 0;
    }


    return;
}




static rt_err_t read_point(touch_msg_t p_msg)
{
    int res;
    unsigned char out_val[3];
    uint8_t tp_num;
    rt_err_t err;

    LOG_D("ft6336 read_point");
    rt_touch_irq_pin_enable(1);

    res = 0;
    //LOG_I("tpnum:%d",tp_num);
    if (ft_bus && p_msg)
    {
        err = read_regs(FT_TD_STATUS, 1, &tp_num);
        if (RT_EOK != err)
        {
            goto ERROR_HANDLE;
        }

        if (tp_num > 0)
        {
            // get x positon
            err = read_regs(FT_P1_XL, 1, &out_val[0]);
            if (RT_EOK == err)
            {
                //LOG_D("outx 0x%02x, 0x%02x, 0x%02x\n", out_val[0], out_val[1], out_val[2]);
                p_msg->x = out_val[0];
            }
            else
            {
                LOG_I("get x fail\n");
                res = 1;
            }

            // get y position
            err = read_regs(FT_P1_YL, 1, &out_val[0]);
            if (RT_EOK == err)
            {
                //LOG_D("outy 0x%02x, 0x%02x, 0x%02x\n", out_val[0], out_val[1], out_val[2]);
                p_msg->y = out_val[0];
            }
            else
            {
                LOG_I("get y fail\n");
                res = 2;
            }

            p_msg->event = TOUCH_EVENT_DOWN;
            ft6336_correct_pos(p_msg);


            LOG_D("Down event, x = %d, y = %d\n", p_msg->x, p_msg->y);

            return (tp_num > 1) ? RT_EOK : RT_EEMPTY;
        }
        else
        {
            res = 1;
            p_msg->x = 0;
            p_msg->y = 0;
            p_msg->event = TOUCH_EVENT_UP;
            LOG_D("Up event, x = %d, y = %d\n", p_msg->x, p_msg->y);
            return RT_EEMPTY;
        }
    }
    else
    {
        //LOG_I("spi or handle error\n");
        res = 1;
    }

ERROR_HANDLE:
    p_msg->x = 0;
    p_msg->y = 0;
    p_msg->event = TOUCH_EVENT_UP;
    LOG_E("Error, return Up event, x = %d, y = %d\n", p_msg->x, p_msg->y);

    return RT_ERROR;
}


void ft6336_irq_handler(void *arg)
{
    rt_err_t ret = RT_ERROR;

    int value = (int)arg;
    LOG_D("ft6336 touch_irq_handler\n");

    rt_touch_irq_pin_enable(0);

    ret = rt_sem_release(ft6336_driver.isr_sem);
    RT_ASSERT(RT_EOK == ret);
}


static rt_err_t init(void)
{
    rt_err_t err;
    struct touch_message msg;

    LOG_D("ft6336 init");

    rt_touch_irq_pin_attach(PIN_IRQ_MODE_FALLING, ft6336_irq_handler, NULL);
    rt_touch_irq_pin_enable(1); //Must enable before read I2C


    err = write_reg(FT_ID_G_MODE, 1);
    if (RT_EOK != err)
    {
        LOG_E("G_MODE set fail\n");
        //return RT_FALSE;
    }


    LOG_D("ft6336 init OK");
    return RT_EOK;

}

static rt_err_t deinit(void)
{
    LOG_D("ft6336 deinit");

    rt_touch_irq_pin_enable(0);
    return RT_EOK;

}

static rt_bool_t probe(void)
{

    ft_bus = (struct rt_i2c_bus_device *)rt_device_find(TOUCH_DEVICE_NAME);
    if (RT_Device_Class_I2CBUS != ft_bus->parent.type)
    {
        ft_bus = NULL;
    }
    if (ft_bus)
    {
        rt_device_open((rt_device_t)ft_bus, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
    }
    else
    {
        LOG_I("bus not find\n");
        return RT_FALSE;
    }

    {
        struct rt_i2c_configuration configuration =
        {
            .mode = 0,
            .addr = 0,
            .timeout = 500,
            .max_hz  = 400000,
        };

        rt_i2c_configure(ft_bus, &configuration);
    }




    LOG_I("ft6336 probe OK");

    return RT_TRUE;
}


static struct touch_ops ops =
{
    read_point,
    init,
    deinit
};



static int rt_ft6336_init(void)
{

    ft6336_driver.probe = probe;
    ft6336_driver.ops = &ops;
    ft6336_driver.user_data = RT_NULL;
    ft6336_driver.isr_sem = rt_sem_create("ft6336", 0, RT_IPC_FLAG_FIFO);

    rt_touch_drivers_register(&ft6336_driver);

    return 0;

}
INIT_COMPONENT_EXPORT(rt_ft6336_init);

//#define FT6336_FUNC_TEST
#ifdef FT6336_FUNC_TEST

int cmd_ft_test(int argc, char *argv[])
{
    touch_data_t post = {0};
    int res, looper;

    if (argc > 1)
    {
        looper = atoi(argv[1]);
    }
    else
    {
        looper = 0x0fffffff;
    }

    if (NULL == ft_bus)
    {
        ft6336_init();
    }
    while (looper != 0)
    {
        res = touch_read(&post);
        if (post.state)
        {
            LOG_I("x = %d, y = %d", post.point.x, post.point.y);
        }

        looper--;
        rt_thread_delay(100);
    }
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_ft_test, __cmd_ft_test, Test hw ft6336);
#endif  /* ADS7846_FUNC_TEST */

/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/
