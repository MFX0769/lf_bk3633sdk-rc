
#include "aes.h"
#include "icu.h"
#include "bk3633_reglist.h"
#include "user_config.h"



uint8_t aes_ok=0;
#define UART_PRINTF    uart_printf


uint8_t encryp_data[16];



void rwip_aes_encrypt(const uint8_t *key, const uint8_t* val,uint8_t mode)
{

    ip_aescntl_aes_mode_setf(mode&0x01);

    // Copy data to EM buffer
    em_wr(val, EM_ENC_IN_OFFSET, 16);

    // copy the key in the register dedicated for the encryption
    ip_aeskey31_0_set(  co_read32p(&(key[0])));
    ip_aeskey63_32_set( co_read32p(&(key[4])));
    ip_aeskey95_64_set( co_read32p(&(key[8])));
    ip_aeskey127_96_set(co_read32p(&(key[12])));

    // Set the pointer on the data to encrypt.
    ip_aesptr_setf(EM_ENC_IN_OFFSET >> 2);

    // enable crypt interrupt (and clear a previous interrupt if needed)
    ip_intack1_cryptintack_clearf(1);
    ip_intcntl1_cryptintmsk_setf(1);

    // start the encryption
    ip_aescntl_aes_start_setf(1);
}






void rwip_isr(void)
{

    // Check interrupt status and call the appropriate handlers
    uint32_t irq_stat      = ip_intstat1_get();


    if (irq_stat & IP_SLPINTSTAT_BIT)
    {
        ip_intack1_slpintack_clearf(1);

    }
    // Encryption interrupt
    if (irq_stat & IP_CRYPTINTSTAT_BIT)
    {

        ip_intack1_cryptintack_clearf(1);
    //    UART_PRINTF("rwip 2\r\n");
        aes_ok =1;
        ip_intcntl1_cryptintmsk_setf(0);
    //    em_rd(encryp_data, EM_ENC_OUT_OFFSET, 16);

    }


}




void app_aes_test(uint8_t *key, uint8_t *value,uint8_t *encrypted)
{
uint8_t i,enc_data[16];
uint8_t tmp=0;
    setf_SYS_Reg0x10_int_rwdm_en; //enable int rwdm
    setf_SYS_Reg0x11_int_rwdm_pri; // 1:fiq
    SET_RWBT_POWER_UP;
    UART_PRINTF("key    = ");
    for(i=0;i<16;i++)
        UART_PRINTF("%x,",key[i]);
    UART_PRINTF("\r\n");


    rwip_aes_encrypt(key,value,AES_ENCODE);

    UART_PRINTF("value  = ");
    for(i=0;i<16;i++)
        UART_PRINTF("%x,",value[i]);
    UART_PRINTF("\r\n");

 //   app_aes_start(key,value);
    while(1)
    {
        if(aes_ok)
        {
            aes_ok = 0;
            em_rd(enc_data, EM_ENC_OUT_OFFSET, 16);

            rwip_aes_encrypt(key,encrypted,AES_DECODE);

            UART_PRINTF("enc    = ");
            for(i=0;i<16;i++)
            UART_PRINTF("%x,",enc_data[i]);
            UART_PRINTF("\r\n");

            while(1)
            {
                if(aes_ok)
                {
                    aes_ok = 0;
                    em_rd(enc_data, EM_ENC_OUT_OFFSET, 16);

                    UART_PRINTF("decode = ");
                    for(i=0;i<16;i++)
                    UART_PRINTF("%x,",enc_data[i]);
                    UART_PRINTF("\r\n");
                    tmp =1;


                }
                if (tmp )
                    break;
            }

        }
        if (tmp )
            break;

    }
    clrf_SYS_Reg0x10_int_rwdm_en; //enable int rwdm
    SET_RWBT_POWER_DOWN;
    UART_PRINTF("end\r\n");

}


